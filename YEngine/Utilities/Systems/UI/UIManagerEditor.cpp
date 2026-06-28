#include "UIManager.h"
#include "UIBase.h"

#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_internal.h>
#endif
#include <Loaders/Texture/TextureManager.h>

// C++
#include <algorithm>
#include <set>
#include <map>
#include <vector>
#include <string>
#include <filesystem>
#include <climits>
#include <cstring>

// ============================================================
// UIManager の ImGui エディタ部
// ============================================================
// UIManager 本体（管理ロジック）から ImGui エディタUIを分離したファイル。
// 各関数は UIManager のメンバ定義のままなので private へ自由にアクセスできる。
// 本体は内部 #ifdef USE_IMGUI でガードしているため Release では空関数になる。
// ============================================================

namespace YoRigine {
	/*==================================================================
							ImGuiデバッグ
	===================================================================*/

	void UIManager::ImGuiDebug() {
#ifdef USE_IMGUI

		// ===================== [BEGIN] Drag-Reorder State =====================
		static std::string g_DraggingId;
		static bool g_IsDragging = false;
		bool requestCommitDrag = false;
		static int g_DragSrcIndex = -1;
		static int g_DragInsertIndex = -1;
		static int g_DragRestrictLayer = INT_MIN; // レイヤー固定（レイヤータブ時は同レイヤー内）
		const float kLongPressSec = 0.25f;

		auto IndexOfId = [&](const std::string& id) -> int {
			auto it = std::find(drawOrder_.begin(), drawOrder_.end(), id);
			return (it == drawOrder_.end()) ? -1 : static_cast<int>(std::distance(drawOrder_.begin(), it));
			};

		// 既存: 単体移動
		auto MoveToIndex = [&](int src, int dst, int restrictLayer) {
			if (src < 0 || src >= static_cast<int>(drawOrder_.size())) return;
			dst = std::max(0, std::min(dst, static_cast<int>(drawOrder_.size())));

			if (restrictLayer != INT_MIN) {
				int first = -1, last = -1; // [first, last)
				for (int i = 0; i < static_cast<int>(drawOrder_.size()); ++i) {
					auto it = uiElements_.find(drawOrder_[i]);
					if (it == uiElements_.end() || !it->second) continue;
					if (it->second->GetLayer() == restrictLayer) {
						if (first == -1) first = i;
						last = i + 1;
					}
				}
				if (first != -1 && last != -1) {
					dst = std::max(first, std::min(dst, last));
				}
			}

			if (dst == src || dst == src + 1) return;
			std::string movingId = drawOrder_[src];
			drawOrder_.erase(drawOrder_.begin() + src);
			if (dst > src) dst--;
			drawOrder_.insert(drawOrder_.begin() + dst, std::move(movingId));
			};
		// ===================== [END] Drag-Reorder State =====================

		// ===================== [BEGIN] Multi-Select State =====================
		static std::set<std::string> g_SelectedIds;
		static std::string g_AnchorId;
		auto SetSingleSelection = [&](const std::string& id) {
			g_SelectedIds.clear();
			g_SelectedIds.insert(id);
			g_AnchorId = id;
			selectedUIId_ = id;
			};
		// ===================== [END] Multi-Select State =====================

		// ===================== [BEGIN] Block-Drag (group move) =====================
		// 複数選択の塊を移動するための状態とヘルパ
		static bool g_IsBlockDrag = false;                // 今回のドラッグが塊移動か
		static std::string g_InsertAfterIdTarget;         // ドロップ時に、このIDの直後へ挿入（空なら先頭）
		// 選択集合を現在の描画順で並べ替えた配列を作成
		auto BuildSelectedBlockOrdered = [&]() {
			std::vector<std::string> block;
			block.reserve(g_SelectedIds.size());
			for (auto& id : drawOrder_) {
				if (g_SelectedIds.count(id)) block.push_back(id);
			}
			return block;
			};
		// レイヤー制限の範囲取得 [first,last)
		auto GetLayerRange = [&](int restrictLayer) -> std::pair<int, int> {
			if (restrictLayer == INT_MIN) return { 0, static_cast<int>(drawOrder_.size()) };
			int first = -1, last = -1;
			for (int i = 0; i < static_cast<int>(drawOrder_.size()); ++i) {
				auto it = uiElements_.find(drawOrder_[i]);
				if (it == uiElements_.end() || !it->second) continue;
				if (it->second->GetLayer() == restrictLayer) {
					if (first == -1) first = i;
					last = i + 1;
				}
			}
			if (first == -1) return { 0,0 };
			return { first,last };
			};
		// ドラッグ中、プレビュー位置から「非選択アイテム基準の直後ターゲットID」を更新
		auto UpdateInsertAfterTarget = [&](const std::string& hoveredId, bool insertBefore, int restrictLayer) {
			// 対象範囲（レイヤー表示時は該当レイヤー範囲）で非選択リストを作る
			auto range = GetLayerRange(restrictLayer);
			std::vector<std::string> nonSelected;
			nonSelected.reserve(range.second - range.first);
			for (int i = range.first; i < range.second; ++i) {
				const std::string& id = drawOrder_[i];
				if (!g_IsBlockDrag || g_SelectedIds.count(id) == 0) {
					nonSelected.push_back(id);
				}
			}

			// hoveredIdの描画順インデックス
			int hoverIdx = IndexOfId(hoveredId);
			if (hoverIdx < 0) return;

			// 探索関数（nonSelected側の直前/直後を探す）
			auto findPrevNonSel = [&](int idx) -> std::string {
				for (int i = idx - 1; i >= range.first; --i) {
					const std::string& cand = drawOrder_[i];
					if (std::find(nonSelected.begin(), nonSelected.end(), cand) != nonSelected.end())
						return cand;
				}
				return std::string(); // none -> 先頭
				};
			auto findCurrOrNextNonSel = [&](int idx) -> std::string {
				for (int i = idx; i < range.second; ++i) {
					const std::string& cand = drawOrder_[i];
					if (std::find(nonSelected.begin(), nonSelected.end(), cand) != nonSelected.end())
						return cand;
				}
				// none -> nonSelectedの最後の要素の直後に入れたいので、それを返す
				if (!nonSelected.empty()) return nonSelected.back();
				return std::string();
				};

			if (insertBefore) {
				// 行の上線＝その直前の非選択アイテムの「直後」に入れる（なければ先頭）
				g_InsertAfterIdTarget = findPrevNonSel(hoverIdx);
			} else {
				// 行の下線＝その位置以降の最初の非選択アイテムの「直後」に入れる
				g_InsertAfterIdTarget = findCurrOrNextNonSel(hoverIdx);
			}
			};
		// 塊を「ターゲットIDの直後」に挿入
		auto CommitBlockMove = [&](int restrictLayer) {
			// 非選択リストの直後へ、選択塊をまとめて挿入する
			std::vector<std::string> order = drawOrder_;
			// 1) 選択塊を現在の順序で構築し、いったん取り除く
			auto block = BuildSelectedBlockOrdered();
			if (block.empty()) return;
			// レイヤー表示時は塊内がすべて同一レイヤーであること（ドラッグ開始時にチェック済み想定）

			// remove selected
			order.erase(std::remove_if(order.begin(), order.end(),
				[&](const std::string& id) { return g_SelectedIds.count(id) != 0; }), order.end());

			// 2) 挿入位置を決定
			int dst = 0;
			if (restrictLayer != INT_MIN) {
				auto range = GetLayerRange(restrictLayer);
				dst = range.first; // デフォルトはそのレイヤー先頭
			}
			if (!g_InsertAfterIdTarget.empty()) {
				auto it = std::find(order.begin(), order.end(), g_InsertAfterIdTarget);
				if (it != order.end()) {
					dst = static_cast<int>(std::distance(order.begin(), it)) + 1;
				} else {
					// ターゲットが見つからなければ末尾に
					dst = static_cast<int>(order.size());
				}
			}

			// レイヤー範囲にクランプ
			if (restrictLayer != INT_MIN) {
				auto range = GetLayerRange(restrictLayer);
				dst = std::max(range.first, std::min(dst, range.second));
			} else {
				dst = std::max(0, std::min(dst, static_cast<int>(order.size())));
			}

			// 3) 相対順を維持したまま挿入
			order.insert(order.begin() + dst, block.begin(), block.end());

			// 4) 反映
			drawOrder_ = std::move(order);
			};
		// ===================== [END] Block-Drag (group move) =====================

		// タブバーで整理
		if (ImGui::BeginTabBar("UIManagerTabs")) {

			if (ImGui::BeginTabItem("UI編集")) {

				ImGui::BeginChild("UIList", ImVec2(ImGui::GetContentRegionAvail().x * 0.25f, 0), true);

				ImGui::Text("UI一覧 (%zu個)", uiElements_.size());
				ImGui::Separator();

				if (ImGui::Button("新規作成", ImVec2(-1, 0))) {
					std::string newId = GenerateUniqueID("NewUI");
					auto newUI = std::make_unique<UIBase>(newId);
					newUI->Initialize("./Resources/UIConfigs/" + newId + ".json");
					AddUI(newId, std::move(newUI));
					SetSingleSelection(newId);
				}

				if (ImGui::Button("全て表示", ImVec2(-1, 0))) ShowAll(true);
				if (ImGui::Button("全て非表示", ImVec2(-1, 0))) ShowAll(false);

				ImGui::Separator();
				ImGui::TextDisabled("Shift+クリックで範囲選択、Ctrl+クリックで追加/解除");
				if (!g_SelectedIds.empty()) { ImGui::SameLine(); ImGui::TextDisabled("| 選択中: %d", (int)g_SelectedIds.size()); }

				ImGui::Separator();

				static char filterText[128] = "";
				ImGui::InputTextWithHint("##filter", "検索...", filterText, sizeof(filterText));

				static bool sortByLayer = true;
				ImGui::Checkbox("レイヤーでグループ化", &sortByLayer);
				ImGui::SameLine();
				ImGui::TextDisabled("長押し(0.25s)してドラッグで並べ替え");

				ImGui::Separator();

				std::string uiToDelete = "";

				if (sortByLayer) {
					std::map<int, std::vector<std::pair<std::string, UIBase*>>> uiByLayer;
					for (const auto& id : drawOrder_) {
						auto it = uiElements_.find(id);
						if (it == uiElements_.end() || !it->second) continue;
						auto* ui = it->second.get();
						if (strlen(filterText) > 0) {
							if (ui->GetName().find(filterText) == std::string::npos &&
								id.find(filterText) == std::string::npos) continue;
						}
						uiByLayer[ui->GetLayer()].push_back({ id, ui });
					}

					for (auto& [layer, uis] : uiByLayer) {
						ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.4f, 0.6f, 0.8f));
						bool layerOpen = ImGui::TreeNodeEx((void*)(intptr_t)layer,
							ImGuiTreeNodeFlags_DefaultOpen, "レイヤー %d (%zu個)", layer, uis.size());
						ImGui::PopStyleColor();

						if (layerOpen) {
							std::string moveUpId = "";
							std::string moveDownId = "";

							for (size_t i = 0; i < uis.size(); ++i) {
								auto& [id, ui] = uis[i];
								ImGui::PushID(id.c_str());

								bool isSelected = (g_SelectedIds.count(id) != 0);
								bool visible = ui->IsVisible();

								ImGui::Indent(16.0f);

								ImGui::BeginGroup();
								if (i > 0) {
									if (ImGui::ArrowButton("##up", ImGuiDir_Up)) moveUpId = id;
									if (ImGui::IsItemHovered()) ImGui::SetTooltip("レイヤー内で前面へ");
								} else {
									ImGui::Dummy(ImVec2(18, 18));
								}
								ImGui::SameLine(0, 2);
								if (i < uis.size() - 1) {
									if (ImGui::ArrowButton("##down", ImGuiDir_Down)) moveDownId = id;
									if (ImGui::IsItemHovered()) ImGui::SetTooltip("レイヤー内で背面へ");
								} else {
									ImGui::Dummy(ImVec2(18, 18));
								}
								ImGui::EndGroup();

								ImGui::SameLine();

								if (ImGui::Checkbox("##visible", &visible)) ui->SetVisible(visible);

								ImGui::SameLine();

								// Selectable（選択描画）
								if (ImGui::Selectable(ui->GetName().c_str(), isSelected)) {
									// クリック処理は下で
								}

								// 複数選択クリック処理（Layered）
								if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
									ImGuiIO& io = ImGui::GetIO();
									bool shift = io.KeyShift;
									bool ctrl = io.KeyCtrl || io.KeySuper;

									int anchorPos = -1;
									if (!g_AnchorId.empty()) {
										for (size_t k = 0; k < uis.size(); ++k) {
											if (uis[k].first == g_AnchorId) { anchorPos = (int)k; break; }
										}
									}

									if (shift) {
										if (!ctrl) g_SelectedIds.clear();
										if (anchorPos == -1) g_SelectedIds.insert(id);
										else {
											int start = std::min(anchorPos, (int)i);
											int end = std::max(anchorPos, (int)i);
											for (int k = start; k <= end; ++k) g_SelectedIds.insert(uis[k].first);
										}
										g_AnchorId = id;
										selectedUIId_ = id;
									} else if (ctrl) {
										if (isSelected) g_SelectedIds.erase(id);
										else g_SelectedIds.insert(id);
										g_AnchorId = id;
										selectedUIId_ = id;
									} else {
										SetSingleSelection(id);
									}
								}

								// 右クリックメニュー
								if (ImGui::BeginPopupContextItem()) {
									if (ImGui::MenuItem("削除")) uiToDelete = id;
									if (ImGui::MenuItem("複製")) {
										std::string newId = GenerateUniqueID(id);
										auto newUI = std::make_unique<UIBase>(newId);
										newUI->Initialize("./Resources/UIConfigs/" + newId + ".json");
										newUI->CopyPropertiesFrom(ui);
										AddUI(newId, std::move(newUI));
									}
									ImGui::Separator();
									if (ImGui::MenuItem("前面へ移動")) moveUpId = id;
									if (ImGui::MenuItem("背面へ移動")) moveDownId = id;
									ImGui::EndPopup();
								}

								// プレビュー＆ドラッグ（Layered）
								{
									ImGuiIO& io = ImGui::GetIO();
									ImRect rowRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

									// 長押し開始
									if (!g_IsDragging) {
										if (ImGui::IsItemHovered() && io.MouseDown[0] && io.MouseDownDuration[0] > kLongPressSec) {
											g_IsDragging = true;
											g_DraggingId = id;
											g_DragSrcIndex = IndexOfId(id);
											g_DragInsertIndex = g_DragSrcIndex;
											g_DragRestrictLayer = layer; // 同レイヤー限定

											// 塊ドラッグ判定（同一レイヤーであること）
											g_IsBlockDrag = (g_SelectedIds.size() > 1 && g_SelectedIds.count(id) != 0);
											if (g_IsBlockDrag) {
												for (const auto& sid : g_SelectedIds) {
													auto it = uiElements_.find(sid);
													if (it == uiElements_.end() || !it->second) { g_IsBlockDrag = false; break; }
													if (it->second->GetLayer() != layer) { g_IsBlockDrag = false; break; }
												}
											}
											g_InsertAfterIdTarget.clear();
											ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
										}
									}

									// ドラッグ中プレビュー
									if (g_IsDragging && io.MouseDown[0]) {
										if (ImGui::IsMouseHoveringRect(rowRect.Min, rowRect.Max)) {
											int targetIndex = IndexOfId(id);
											float midY = (rowRect.Min.y + rowRect.Max.y) * 0.5f;
											bool insertBefore = (io.MousePos.y < midY);
											g_DragInsertIndex = insertBefore ? targetIndex : (targetIndex + 1);

											// 塊ドラッグ時は「非選択の直後ターゲット」を更新
											if (g_IsBlockDrag) {
												UpdateInsertAfterTarget(id, insertBefore, g_DragRestrictLayer);
											}

											auto* dl = ImGui::GetWindowDrawList();
											float lineY = insertBefore ? rowRect.Min.y : rowRect.Max.y;
											dl->AddLine(ImVec2(rowRect.Min.x, lineY), ImVec2(rowRect.Max.x, lineY),
												IM_COL32(255, 200, 0, 230), 2.0f);
										}
									}

									if (g_IsDragging && io.MouseReleased[0]) {
										requestCommitDrag = true;
									}
								}

								ImGui::Unindent(16.0f);
								ImGui::PopID();
							}

							// 既存の上下移動
							if (!moveUpId.empty() && !moveDownId.empty()) moveDownId.clear();
							if (!moveUpId.empty()) MoveDrawOrderForward(moveUpId);
							if (!moveDownId.empty()) MoveDrawOrderBackward(moveDownId);

							ImGui::TreePop();
						}
					}
				} else {
					// 通常（フラット）
					std::vector<std::pair<std::string, UIBase*>> sortedUIs;
					for (const auto& id : drawOrder_) {
						auto it = uiElements_.find(id);
						if (it != uiElements_.end() && it->second) {
							auto* ui = it->second.get();
							if (strlen(filterText) > 0) {
								if (ui->GetName().find(filterText) == std::string::npos &&
									id.find(filterText) == std::string::npos) continue;
							}
							sortedUIs.push_back({ id, ui });
						}
					}

					std::string moveUpId = "";
					std::string moveDownId = "";

					for (size_t i = 0; i < sortedUIs.size(); ++i) {
						auto& [id, ui] = sortedUIs[i];
						ImGui::PushID(id.c_str());

						bool isSelected = (g_SelectedIds.count(id) != 0);
						bool visible = ui->IsVisible();

						ImGui::BeginGroup();
						if (i > 0) {
							if (ImGui::ArrowButton("##up", ImGuiDir_Up)) moveUpId = id;
							if (ImGui::IsItemHovered()) ImGui::SetTooltip("前面へ");
						} else {
							ImGui::Dummy(ImVec2(18, 18));
						}
						ImGui::SameLine(0, 2);
						if (i < sortedUIs.size() - 1) {
							if (ImGui::ArrowButton("##down", ImGuiDir_Down)) moveDownId = id;
							if (ImGui::IsItemHovered()) ImGui::SetTooltip("背面へ");
						} else {
							ImGui::Dummy(ImVec2(18, 18));
						}
						ImGui::EndGroup();
						ImGui::SameLine();

						if (ImGui::Checkbox("##visible", &visible)) ui->SetVisible(visible);

						ImGui::SameLine();

						std::string displayName = ui->GetName() + " [L:" + std::to_string(ui->GetLayer()) + "]";
						if (ImGui::Selectable(displayName.c_str(), isSelected)) {
							// クリック処理は下
						}

						// 複数選択クリック処理（Flat）
						if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
							ImGuiIO& io = ImGui::GetIO();
							bool shift = io.KeyShift;
							bool ctrl = io.KeyCtrl || io.KeySuper;

							int anchorPos = -1;
							if (!g_AnchorId.empty()) {
								for (size_t k = 0; k < sortedUIs.size(); ++k) {
									if (sortedUIs[k].first == g_AnchorId) { anchorPos = (int)k; break; }
								}
							}

							if (shift) {
								if (!ctrl) g_SelectedIds.clear();
								if (anchorPos == -1) g_SelectedIds.insert(id);
								else {
									int start = std::min(anchorPos, (int)i);
									int end = std::max(anchorPos, (int)i);
									for (int k = start; k <= end; ++k) g_SelectedIds.insert(sortedUIs[k].first);
								}
								g_AnchorId = id;
								selectedUIId_ = id;
							} else if (ctrl) {
								if (isSelected) g_SelectedIds.erase(id);
								else g_SelectedIds.insert(id);
								g_AnchorId = id;
								selectedUIId_ = id;
							} else {
								SetSingleSelection(id);
							}
						}

						// 右クリック
						if (ImGui::BeginPopupContextItem()) {
							if (ImGui::MenuItem("削除")) uiToDelete = id;
							if (ImGui::MenuItem("複製")) {
								std::string newId = GenerateUniqueID(id);
								auto newUI = std::make_unique<UIBase>(newId);
								newUI->Initialize("./Resources/UIConfigs/" + newId + ".json");
								newUI->CopyPropertiesFrom(ui);
								AddUI(newId, std::move(newUI));
							}
							ImGui::Separator();
							if (ImGui::MenuItem("前面へ移動")) moveUpId = id;
							if (ImGui::MenuItem("背面へ移動")) moveDownId = id;
							ImGui::EndPopup();
						}

						// プレビュー＆ドラッグ（Flat）
						{
							ImGuiIO& io = ImGui::GetIO();
							ImRect rowRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

							if (!g_IsDragging) {
								if (ImGui::IsItemHovered() && io.MouseDown[0] && io.MouseDownDuration[0] > kLongPressSec) {
									g_IsDragging = true;
									g_DraggingId = id;
									g_DragSrcIndex = IndexOfId(id);
									g_DragInsertIndex = g_DragSrcIndex;
									g_DragRestrictLayer = INT_MIN; // 制限なし

									// 複数選択なら塊ドラッグ
									g_IsBlockDrag = (g_SelectedIds.size() > 1 && g_SelectedIds.count(id) != 0);
									g_InsertAfterIdTarget.clear();
									ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
								}
							}

							if (g_IsDragging && io.MouseDown[0]) {
								if (ImGui::IsMouseHoveringRect(rowRect.Min, rowRect.Max)) {
									int targetIndex = IndexOfId(id);
									float midY = (rowRect.Min.y + rowRect.Max.y) * 0.5f;
									bool insertBefore = (io.MousePos.y < midY);
									g_DragInsertIndex = insertBefore ? targetIndex : (targetIndex + 1);

									if (g_IsBlockDrag) {
										UpdateInsertAfterTarget(id, insertBefore, g_DragRestrictLayer);
									}

									auto* dl = ImGui::GetWindowDrawList();
									float lineY = insertBefore ? rowRect.Min.y : rowRect.Max.y;
									dl->AddLine(ImVec2(rowRect.Min.x, lineY), ImVec2(rowRect.Max.x, lineY),
										IM_COL32(255, 200, 0, 230), 2.0f);
								}
							}

							if (g_IsDragging && io.MouseReleased[0]) requestCommitDrag = true;
						}

						ImGui::PopID();
					}

					if (!moveUpId.empty()) MoveDrawOrderForward(moveUpId);
					if (!moveDownId.empty()) MoveDrawOrderBackward(moveDownId);
				}

				// 削除
				if (!uiToDelete.empty()) {
					RemoveUI(uiToDelete);
					if (selectedUIId_ == uiToDelete) selectedUIId_.clear();
					g_SelectedIds.erase(uiToDelete);
				}

				// ドラッグ確定
				if (requestCommitDrag && g_IsDragging) {
					if (g_IsBlockDrag) {
						CommitBlockMove(g_DragRestrictLayer);
					} else {
						MoveToIndex(g_DragSrcIndex, g_DragInsertIndex, g_DragRestrictLayer);
					}
					g_IsDragging = false;
					g_IsBlockDrag = false;
					g_DraggingId.clear();
					g_DragSrcIndex = -1;
					g_DragInsertIndex = -1;
					g_DragRestrictLayer = INT_MIN;
					g_InsertAfterIdTarget.clear();
				}

				// フローティングラベル
				if (g_IsDragging) {
					auto* fg = ImGui::GetForegroundDrawList();
					std::string label = g_IsBlockDrag ? ("移動(塊): " + std::to_string((int)g_SelectedIds.size()) + "件")
						: ("移動: " + g_DraggingId);
					ImVec2 mousePos = ImGui::GetIO().MousePos;
					ImVec2 pos = ImVec2(mousePos.x + 12.0f, mousePos.y + 12.0f);
					ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
					ImVec2 rectMax = ImVec2(pos.x + textSize.x + 12.0f, pos.y + textSize.y + 8.0f);
					fg->AddRectFilled(pos, rectMax, IM_COL32(30, 30, 30, 220), 4.0f);
					fg->AddText(ImVec2(pos.x + 6.0f, pos.y + 4.0f), IM_COL32(255, 255, 255, 255), label.c_str());
					ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
				}

				ImGui::EndChild();

				ImGui::SameLine();

				// 右側 詳細（既存）
				ImGui::BeginChild("UIDetails", ImVec2(0, 0), true);

				if (!selectedUIId_.empty()) {
					auto* selectedUI = GetUI(selectedUIId_);
					if (selectedUI) {
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 1.0f, 1.0f));
						ImGui::Text("編集中: %s", selectedUI->GetName().c_str());
						ImGui::PopStyleColor();

						ImGui::SameLine();
						ImGui::TextDisabled("(ID: %s)", selectedUIId_.c_str());

						ImGui::Separator();
						ImGui::Spacing();

						ImGui::BeginChild("EditArea", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

						ImGui::Separator();
						ImGui::Text("ID 設定");

						static char idEditBuf[128] = {};
						static std::string lastSelectedForBuf;
						if (lastSelectedForBuf != selectedUIId_) {
							memset(idEditBuf, 0, sizeof(idEditBuf));
							strncpy_s(idEditBuf, selectedUIId_.c_str(), sizeof(idEditBuf) - 1);
							lastSelectedForBuf = selectedUIId_;
						}

						ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 120.0f);
						ImGui::InputText("##id_edit", idEditBuf, sizeof(idEditBuf));
						ImGui::SameLine();
						if (ImGui::Button("ID変更", ImVec2(110, 0))) {
							std::string newId = idEditBuf;
							if (!newId.empty() && newId != selectedUIId_) {
								if (RenameUI(selectedUIId_, newId)) {
									strncpy_s(idEditBuf, selectedUIId_.c_str(), sizeof(idEditBuf) - 1);
								}
							}
						}

						ImGui::TextDisabled("保存先プレビュー: %s%s.json", UI_CONFIG_DIRECTORY.c_str(), selectedUIId_.c_str());

						ImGui::Spacing();
						ImGui::Separator();

						DisplayImprovedTextureSelector(selectedUI);
						selectedUI->ImGUi();

						ImGui::EndChild();
					}
				} else {
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
					ImGui::SetCursorPosY(ImGui::GetWindowHeight() * 0.4f);
					float textWidth = ImGui::CalcTextSize("UIを選択してください").x;
					ImGui::SetCursorPosX((ImGui::GetWindowWidth() - textWidth) * 0.5f);
					ImGui::Text("UIを選択してください");
					ImGui::PopStyleColor();
				}

				ImGui::EndChild();

				ImGui::EndTabItem();
			}

			// 以下シーン管理/グループ/統計タブは既存のまま
			if (ImGui::BeginTabItem("シーン管理")) {

				if (!currentSceneName_.empty()) {

					// ボタンを少し目立たせる（薄い緑色など）
					ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor(40, 120, 40));

					// 「(現在のシーン名) を上書き保存」というボタンを表示
					std::string overwriteLabel = "「" + currentSceneName_ + "」を上書き保存";
					if (ImGui::Button(overwriteLabel.c_str(), ImVec2(-1, 40))) {
						SaveScene(currentSceneName_); // 覚えている名前で保存
					}

					ImGui::PopStyleColor(); // 色の設定を元に戻す
					ImGui::Separator();      // 下の機能との区切り線
				}

				static char sceneName[128] = "";
				ImGui::InputTextWithHint("##scenename", "シーン名を入力", sceneName, sizeof(sceneName));
				if (ImGui::Button("現在のレイアウトを保存", ImVec2(-1, 0))) {
					if (strlen(sceneName) > 0) {
						if (SaveScene(sceneName)) ImGui::OpenPopup("SceneSaved");
					}
				}
				if (ImGui::BeginPopupModal("SceneSaved", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
					ImGui::Text("シーンを保存しました!");
					if (ImGui::Button("OK", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
					ImGui::EndPopup();
				}
				ImGui::Separator();
				ImGui::Text("保存済みシーン:");
				auto scenes = GetAvailableScenes();
				for (const auto& scene : scenes) {
					ImGui::PushID(scene.c_str());
					if (ImGui::Button("読込", ImVec2(60, 0))) LoadScene(scene);
					ImGui::SameLine();
					if (ImGui::Button("削除", ImVec2(60, 0))) ImGui::OpenPopup("ConfirmDelete");
					ImGui::SameLine();
					ImGui::Text("%s", scene.c_str());
					if (ImGui::BeginPopupModal("ConfirmDelete", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
						ImGui::Text("シーン '%s' を削除しますか?", scene.c_str());
						ImGui::Separator();
						if (ImGui::Button("はい", ImVec2(120, 0))) { DeleteScene(scene); ImGui::CloseCurrentPopup(); }
						ImGui::SameLine();
						if (ImGui::Button("いいえ", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
						ImGui::EndPopup();
					}
					ImGui::PopID();
				}
				if (scenes.empty()) ImGui::TextDisabled("保存されたシーンがありません");
				ImGui::EndTabItem();
			}

			//if (ImGui::BeginTabItem("グループ管理")) {
			//	static char newGroupName[128] = "";
			//	ImGui::InputTextWithHint("##groupname", "新規グループ名", newGroupName, sizeof(newGroupName));
			//	ImGui::Separator();
			//	for (auto& [groupName, uiIds] : groups_) {
			//		if (ImGui::TreeNode(groupName.c_str())) {
			//			ImGui::Text("UI数: %zu", uiIds.size());
			//			if (ImGui::Button("表示")) ShowGroup(groupName, true);
			//			ImGui::SameLine();
			//			if (ImGui::Button("非表示")) ShowGroup(groupName, false);
			//			ImGui::Separator();
			//			for (const auto& uiId : uiIds) {
			//				auto* ui = GetUI(uiId);
			//				if (ui) ImGui::BulletText("%s", ui->GetName().c_str());
			//			}
			//			ImGui::TreePop();
			//		}
			//	}
			//	if (groups_.empty()) ImGui::TextDisabled("グループがありません");
			//	ImGui::EndTabItem();
			//}

			if (ImGui::BeginTabItem("統計情報")) {
				auto stats = GetStatistics();
				ImGui::Text("総UI数: %d", stats.totalUIs);
				ImGui::Text("表示中: %d", stats.visibleUIs);
				ImGui::Text("非表示: %d", stats.hiddenUIs);
				ImGui::Separator();
				ImGui::Text("レイヤー別:");
				for (const auto& [layer, count] : stats.uisByLayer) {
					ImGui::BulletText("レイヤー %d: %d個", layer, count);
				}
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

#endif
	}

	/*==================================================================
							ヘルパー関数
	===================================================================*/

	void UIManager::DisplayImprovedTextureSelector(UIBase* ui)
	{
		if (!ui) return;
#ifdef USE_IMGUI


		if (ImGui::CollapsingHeader("テクスチャ設定", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Indent(10.0f);

			// =========================================================
			// 現在のテクスチャ
			// =========================================================
			std::string currentTexture = ui->GetTexturePath();

			ImGui::Text("現在のテクスチャ:");
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f),
				currentTexture.empty() ? "(なし)" : currentTexture.c_str());

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// =========================================================
			// 検索バー
			// =========================================================
			static char textureFilter[128] = "";
			ImGui::PushItemWidth(-1);
			ImGui::InputTextWithHint("##texturefilter", "テクスチャを検索...", textureFilter, sizeof(textureFilter));
			ImGui::PopItemWidth();
			ImGui::Spacing();

			// =========================================================
			// ディレクトリ確認
			// =========================================================
			const std::string textureDir = "./Resources/Textures/";
			if (!std::filesystem::exists(textureDir))
			{
				ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "テクスチャフォルダが見つかりません");
				return;
			}

			// =========================================================
			// スクロール領域
			// =========================================================
			ImGui::BeginChild("TextureList", ImVec2(0, 280), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

			auto IsTextureExt = [](const std::string& ext)
				{
					static const std::vector<std::string> validExt = { ".png", ".jpg", ".jpeg", ".bmp", ".tga",".dds" };
					return std::find(validExt.begin(), validExt.end(), ext) != validExt.end();
				};

			// =========================================================
			// 再帰ディレクトリ描画（クリック展開対応）
			// =========================================================
			std::function<void(const std::filesystem::path&)> drawDirectory =
				[&](const std::filesystem::path& path)
				{
					std::vector<std::filesystem::directory_entry> entries;
					for (const auto& e : std::filesystem::directory_iterator(path))
						entries.push_back(e);

					std::sort(entries.begin(), entries.end(),
						[](const auto& a, const auto& b)
						{
							// フォルダ優先、その中で名前順
							if (a.is_directory() != b.is_directory())
								return a.is_directory() > b.is_directory();
							return a.path().filename().string() < b.path().filename().string();
						});

					for (const auto& e : entries)
					{
						std::string name = e.path().filename().string();
						std::string ext = e.path().extension().string();
						std::string rel = std::filesystem::relative(e.path(), textureDir).string();
						std::replace(rel.begin(), rel.end(), '\\', '/');
						std::string fullPath = "Resources/Textures/" + rel;

						ImGui::PushID(fullPath.c_str());

						// ===== フォルダ =====
						if (e.is_directory())
						{
							// ★クリックで展開できるように設定
							bool open = ImGui::TreeNodeEx(
								name.c_str(),
								ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth,
								"[DIR] %s", name.c_str());

							if (open)
							{
								drawDirectory(e.path());
								ImGui::TreePop();
							}
						}
						// ===== テクスチャファイル =====
						else if (e.is_regular_file() && IsTextureExt(ext))
						{
							// 検索フィルタ
							if (strlen(textureFilter) > 0 && name.find(textureFilter) == std::string::npos)
							{
								ImGui::PopID();
								continue;
							}

							bool isSelected = (currentTexture == fullPath);

							if (isSelected)
							{
								ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
								ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.5f, 0.2f, 0.5f));
							}

							if (ImGui::Selectable(name.c_str(), isSelected))
							{
								ui->SetTexture(fullPath);
							}

							if (isSelected)
								ImGui::PopStyleColor(2);
						}

						ImGui::PopID();
					}
				};
			drawDirectory(textureDir);

			ImGui::EndChild();
			ImGui::Unindent(10.0f);
		}
#endif // _DEBUG
	}




	void UIManager::DisplayTextureDirectory(const std::string& path, const std::string& baseDir, UIBase* ui, const char* filter) {
		if (!std::filesystem::exists(path)) return;
#ifdef USE_IMGUI


		// ファイルとディレクトリを分けて収集
		std::vector<std::filesystem::directory_entry> dirs;
		std::vector<std::filesystem::directory_entry> files;

		for (const auto& entry : std::filesystem::directory_iterator(path)) {
			if (entry.is_directory()) {
				dirs.push_back(entry);
			} else if (entry.is_regular_file()) {
				// 画像ファイルのみ
				std::string ext = entry.path().extension().string();
				if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga") {
					files.push_back(entry);
				}
			}
		}

		// ディレクトリをソート
		std::sort(dirs.begin(), dirs.end(), [](const auto& a, const auto& b) {
			return a.path().filename().string() < b.path().filename().string();
			});

		// ファイルをソート
		std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
			return a.path().filename().string() < b.path().filename().string();
			});

		// ディレクトリを階層表示
		for (const auto& entry : dirs) {
			std::string folderName = entry.path().filename().string();

			// フォルダアイコンと色付き表示
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.3f, 1.0f));
			bool isOpen = ImGui::TreeNodeEx(
				entry.path().string().c_str(),
				ImGuiTreeNodeFlags_None,
				"📁 %s",
				folderName.c_str()
			);
			ImGui::PopStyleColor();

			if (isOpen) {
				DisplayTextureDirectory(entry.path().string(), baseDir, ui, filter);
				ImGui::TreePop();
			}
		}

		// ファイルを表示
		for (const auto& entry : files) {
			std::string filename = entry.path().filename().string();
			std::string relativePath = std::filesystem::relative(entry.path(), baseDir).string();

			// フィルター適用
			if (strlen(filter) > 0) {
				if (filename.find(filter) == std::string::npos) {
					continue;
				}
			}

			// 現在選択中のテクスチャはハイライト
			bool isSelected = (ui->GetTexturePath().find(filename) != std::string::npos);

			if (isSelected) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.2f, 0.5f));
			}

			// ファイル拡張子アイコン
			std::string ext = entry.path().extension().string();
			std::string icon = "🖼️";

			// 選択可能なボタンとして表示
			std::string buttonLabel = icon + " " + filename;
			if (ImGui::Selectable(buttonLabel.c_str(), isSelected, 0, ImVec2(-1, 0))) {
				// 相対パスで設定
				std::string fullPath = "./Resources/Textures/" + relativePath;
				// バックスラッシュをスラッシュに変換
				std::replace(fullPath.begin(), fullPath.end(), '\\', '/');
				ui->SetTexture(fullPath);
			}

			if (isSelected) {
				ImGui::PopStyleColor(2);
			}

			// ホバー時に詳細情報を表示
			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				ImGui::Text("ファイル名: %s", filename.c_str());
				ImGui::Text("パス: %s", relativePath.c_str());

				// ファイルサイズを表示
				auto fileSize = std::filesystem::file_size(entry.path());
				if (fileSize < 1024) {
					ImGui::Text("サイズ: %zu bytes", fileSize);
				} else if (fileSize < 1024 * 1024) {
					ImGui::Text("サイズ: %.2f KB", fileSize / 1024.0);
				} else {
					ImGui::Text("サイズ: %.2f MB", fileSize / (1024.0 * 1024.0));
				}

				ImGui::EndTooltip();
			}
		}
#else
		(void)path;
		(void)baseDir;
		(void)ui;
		(void)filter;
#endif
	}
}
