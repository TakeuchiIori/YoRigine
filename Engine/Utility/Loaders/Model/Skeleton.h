#pragma once

// C++
#include <optional>
#include <map>
#include <vector>


// Engine
#include "Joint.h"
#include "Node.h"


// Math
#include "Quaternion.h"
#include "Vector3.h"



class Line;
class Skeleton
{

public:

	/// <summary>
	/// スケルトン作成
	/// </summary>
	/// <param name="rootNode"></param>
	void CreateSkeleton(const Node& rootNode);

	/// <summary>
	/// スケルトンの更新
	/// </summary>
	void UpdateSkeleton();


	/// <summary>
	///  スケルトンの描画　
	/// </summary>
	/// <param name="skeleton"></param>
	void DrawSkeleton(Line& line);

public:

	/// <summary>
	/// スケルトンの取得
	/// </summary>
	/// <returns></returns>
	std::vector<Joint>& GetJoints() { return joints_; }

	/// <summary>
	/// スケルトンの取得
	/// </summary>
	/// <returns></returns>
	std::map<std::string, int32_t>& GetJointMap() { return jointMap_; }

	/// <summary>
	/// スケルトンの取得
	/// </summary>
	/// <returns></returns>
	std::vector<std::pair<int32_t, int32_t>>& GetConnections() { return connections_; }

private:

	int32_t root_;												// RootJointのIndex
	std::map<std::string, int32_t> jointMap_;					// Joint名とIndexとの辞書
	std::vector<Joint> joints_;									// 所属しているジョイント
	std::vector<std::pair<int32_t, int32_t>> connections_;		// 接続情報: 親 -> 子のペア

};

