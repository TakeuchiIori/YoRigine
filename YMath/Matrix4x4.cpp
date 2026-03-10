#include "Matrix4x4.h"
#include <numbers>
#include <MathFunc.h>
#include <DirectXMath.h>


Matrix4x4::Matrix4x4(std::initializer_list<float> list) {
	if (list.size() != 16) {
		throw std::invalid_argument("Matrix4x4 initializer list must contain exactly 16 elements.");
	}
	auto it = list.begin();
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			m[i][j] = *it++;
		}
	}
}

//==================================== 1. 行列の加法 ==============================================
Matrix4x4 Add(Matrix4x4 matrix1, Matrix4x4 matrix2) {
	Matrix4x4 result;
	result.m[0][0] = matrix1.m[0][0] + matrix2.m[0][0];
	result.m[0][1] = matrix1.m[0][1] + matrix2.m[0][1];
	result.m[0][2] = matrix1.m[0][2] + matrix2.m[0][2];
	result.m[0][3] = matrix1.m[0][3] + matrix2.m[0][3];

	result.m[1][0] = matrix1.m[1][0] + matrix2.m[1][0];
	result.m[1][1] = matrix1.m[1][1] + matrix2.m[1][1];
	result.m[1][2] = matrix1.m[1][2] + matrix2.m[1][2];
	result.m[1][3] = matrix1.m[1][3] + matrix2.m[1][3];

	result.m[2][0] = matrix1.m[2][0] + matrix2.m[2][0];
	result.m[2][1] = matrix1.m[2][1] + matrix2.m[2][1];
	result.m[2][2] = matrix1.m[2][2] + matrix2.m[2][2];
	result.m[2][3] = matrix1.m[2][3] + matrix2.m[2][3];

	result.m[3][0] = matrix1.m[3][0] + matrix2.m[3][0];
	result.m[3][1] = matrix1.m[3][1] + matrix2.m[3][1];
	result.m[3][2] = matrix1.m[3][2] + matrix2.m[3][2];
	result.m[3][3] = matrix1.m[3][3] + matrix2.m[3][3];

	return result;
}
//==================================== 2. 行列の減法 ==============================================
Matrix4x4 Subtract(Matrix4x4 matrix1, Matrix4x4 matrix2) {
	Matrix4x4 result;
	result.m[0][0] = matrix1.m[0][0] - matrix2.m[0][0];
	result.m[0][1] = matrix1.m[0][1] - matrix2.m[0][1];
	result.m[0][2] = matrix1.m[0][2] - matrix2.m[0][2];
	result.m[0][3] = matrix1.m[0][3] - matrix2.m[0][3];

	result.m[1][0] = matrix1.m[1][0] - matrix2.m[1][0];
	result.m[1][1] = matrix1.m[1][1] - matrix2.m[1][1];
	result.m[1][2] = matrix1.m[1][2] - matrix2.m[1][2];
	result.m[1][3] = matrix1.m[1][3] - matrix2.m[1][3];

	result.m[2][0] = matrix1.m[2][0] - matrix2.m[2][0];
	result.m[2][1] = matrix1.m[2][1] - matrix2.m[2][1];
	result.m[2][2] = matrix1.m[2][2] - matrix2.m[2][2];
	result.m[2][3] = matrix1.m[2][3] - matrix2.m[2][3];

	result.m[3][0] = matrix1.m[3][0] - matrix2.m[3][0];
	result.m[3][1] = matrix1.m[3][1] - matrix2.m[3][1];
	result.m[3][2] = matrix1.m[3][2] - matrix2.m[3][2];
	result.m[3][3] = matrix1.m[3][3] - matrix2.m[3][3];
	return result;
}
//===================================== 3. 行列の積 ===============================================//
Matrix4x4 Multiply(Matrix4x4 matrix1, Matrix4x4 matrix2) {
	Matrix4x4 result;
	result.m[0][0] = matrix1.m[0][0] * matrix2.m[0][0] + matrix1.m[0][1] * matrix2.m[1][0] + matrix1.m[0][2] * matrix2.m[2][0] + matrix1.m[0][3] * matrix2.m[3][0];
	result.m[1][0] = matrix1.m[1][0] * matrix2.m[0][0] + matrix1.m[1][1] * matrix2.m[1][0] + matrix1.m[1][2] * matrix2.m[2][0] + matrix1.m[1][3] * matrix2.m[3][0];
	result.m[2][0] = matrix1.m[2][0] * matrix2.m[0][0] + matrix1.m[2][1] * matrix2.m[1][0] + matrix1.m[2][2] * matrix2.m[2][0] + matrix1.m[2][3] * matrix2.m[3][0];
	result.m[3][0] = matrix1.m[3][0] * matrix2.m[0][0] + matrix1.m[3][1] * matrix2.m[1][0] + matrix1.m[3][2] * matrix2.m[2][0] + matrix1.m[3][3] * matrix2.m[3][0];

	result.m[0][1] = matrix1.m[0][0] * matrix2.m[0][1] + matrix1.m[0][1] * matrix2.m[1][1] + matrix1.m[0][2] * matrix2.m[2][1] + matrix1.m[0][3] * matrix2.m[3][1];
	result.m[1][1] = matrix1.m[1][0] * matrix2.m[0][1] + matrix1.m[1][1] * matrix2.m[1][1] + matrix1.m[1][2] * matrix2.m[2][1] + matrix1.m[1][3] * matrix2.m[3][1];
	result.m[2][1] = matrix1.m[2][0] * matrix2.m[0][1] + matrix1.m[2][1] * matrix2.m[1][1] + matrix1.m[2][2] * matrix2.m[2][1] + matrix1.m[2][3] * matrix2.m[3][1];
	result.m[3][1] = matrix1.m[3][0] * matrix2.m[0][1] + matrix1.m[3][1] * matrix2.m[1][1] + matrix1.m[3][2] * matrix2.m[2][1] + matrix1.m[3][3] * matrix2.m[3][1];

	result.m[0][2] = matrix1.m[0][0] * matrix2.m[0][2] + matrix1.m[0][1] * matrix2.m[1][2] + matrix1.m[0][2] * matrix2.m[2][2] + matrix1.m[0][3] * matrix2.m[3][2];
	result.m[1][2] = matrix1.m[1][0] * matrix2.m[0][2] + matrix1.m[1][1] * matrix2.m[1][2] + matrix1.m[1][2] * matrix2.m[2][2] + matrix1.m[1][3] * matrix2.m[3][2];
	result.m[2][2] = matrix1.m[2][0] * matrix2.m[0][2] + matrix1.m[2][1] * matrix2.m[1][2] + matrix1.m[2][2] * matrix2.m[2][2] + matrix1.m[2][3] * matrix2.m[3][2];
	result.m[3][2] = matrix1.m[3][0] * matrix2.m[0][2] + matrix1.m[3][1] * matrix2.m[1][2] + matrix1.m[3][2] * matrix2.m[2][2] + matrix1.m[3][3] * matrix2.m[3][2];

	result.m[0][3] = matrix1.m[0][0] * matrix2.m[0][3] + matrix1.m[0][1] * matrix2.m[1][3] + matrix1.m[0][2] * matrix2.m[2][3] + matrix1.m[0][3] * matrix2.m[3][3];
	result.m[1][3] = matrix1.m[1][0] * matrix2.m[0][3] + matrix1.m[1][1] * matrix2.m[1][3] + matrix1.m[1][2] * matrix2.m[2][3] + matrix1.m[1][3] * matrix2.m[3][3];
	result.m[2][3] = matrix1.m[2][0] * matrix2.m[0][3] + matrix1.m[2][1] * matrix2.m[1][3] + matrix1.m[2][2] * matrix2.m[2][3] + matrix1.m[2][3] * matrix2.m[3][3];
	result.m[3][3] = matrix1.m[3][0] * matrix2.m[0][3] + matrix1.m[3][1] * matrix2.m[1][3] + matrix1.m[3][2] * matrix2.m[2][3] + matrix1.m[3][3] * matrix2.m[3][3];

	return result;
}

//===================================== 4. 逆行列 ===============================================//
Matrix4x4 Inverse(const Matrix4x4& m) {

	// データをSIMDレジスタにロード
	// ユーザー定義の構造体ポインタを XMFLOAT4X4* にキャストして読み込みます。
	// XMLoadFloat4x4 はアライメントを気にせず安全にロードしてくれます。
	DirectX::XMMATRIX mat = DirectX::XMLoadFloat4x4(reinterpret_cast<const DirectX::XMFLOAT4X4*>(&m));

	// 逆行列を計算
	// 第1引数は行列式(Determinant)を受け取るベクトルですが、不要なら無視してOKです。
	DirectX::XMVECTOR detVector;
	DirectX::XMMATRIX invMat = XMMatrixInverse(&detVector, mat);

	// 結果をメモリに書き戻す
	Matrix4x4 result;
	XMStoreFloat4x4(reinterpret_cast<DirectX::XMFLOAT4X4*>(&result), invMat);

	return result;
}
//===================================== 5. 転置行列 ===============================================//
Matrix4x4 TransPose(const Matrix4x4& matrix) {
	Matrix4x4 result;
	result.m[0][0] = matrix.m[0][0];
	result.m[0][1] = matrix.m[1][0];
	result.m[0][2] = matrix.m[2][0];
	result.m[0][3] = matrix.m[3][0];

	result.m[1][0] = matrix.m[0][1];
	result.m[1][1] = matrix.m[1][1];
	result.m[1][2] = matrix.m[2][1];
	result.m[1][3] = matrix.m[3][1];

	result.m[2][0] = matrix.m[0][2];
	result.m[2][1] = matrix.m[1][2];
	result.m[2][2] = matrix.m[2][2];
	result.m[2][3] = matrix.m[3][2];

	result.m[3][0] = matrix.m[0][3];
	result.m[3][1] = matrix.m[1][3];
	result.m[3][2] = matrix.m[2][3];
	result.m[3][3] = matrix.m[3][3];

	return result;
}
//===================================== 6. 単位行列 ===============================================//
Matrix4x4 MakeIdentity4x4() {
	Matrix4x4 result;
	result.m[0][0] = 1;
	result.m[0][1] = 0;
	result.m[0][2] = 0;
	result.m[0][3] = 0;

	result.m[1][0] = 0;
	result.m[1][1] = 1;
	result.m[1][2] = 0;
	result.m[1][3] = 0;

	result.m[2][0] = 0;
	result.m[2][1] = 0;
	result.m[2][2] = 1;
	result.m[2][3] = 0;

	result.m[3][0] = 0;
	result.m[3][1] = 0;
	result.m[3][2] = 0;
	result.m[3][3] = 1;

	return result;
}
//=====================================7.拡大縮小行列===============================================//
Matrix4x4 MakeScaleMatrix(const Vector3& scale) {
	Matrix4x4 result;
	result.m[0][0] = scale.x;
	result.m[0][1] = 0;
	result.m[0][2] = 0;
	result.m[0][3] = 0;

	result.m[1][0] = 0;
	result.m[1][1] = scale.y;
	result.m[1][2] = 0;
	result.m[1][3] = 0;

	result.m[2][0] = 0;
	result.m[2][1] = 0;
	result.m[2][2] = scale.z;
	result.m[2][3] = 0;

	result.m[3][0] = 0;
	result.m[3][1] = 0;
	result.m[3][2] = 0;
	result.m[3][3] = 1;

	return result;
}

//=====================================8.平行移動行列===============================================//
Matrix4x4 MakeTranslateMatrix(const Vector3& translate) {
	Matrix4x4 result;
	result.m[0][0] = 1;
	result.m[0][1] = 0;
	result.m[0][2] = 0;
	result.m[0][3] = 0;

	result.m[1][0] = 0;
	result.m[1][1] = 1;
	result.m[1][2] = 0;
	result.m[1][3] = 0;

	result.m[2][0] = 0;
	result.m[2][1] = 0;
	result.m[2][2] = 1;
	result.m[2][3] = 0;

	result.m[3][0] = translate.x;
	result.m[3][1] = translate.y;
	result.m[3][2] = translate.z;
	result.m[3][3] = 1;

	return result;
}





//=====================================9.座標変換===============================================//
Vector3 Transform(const Vector3& vector, const Matrix4x4& matrix) {
	Vector3 result;
	result.x = vector.x * matrix.m[0][0] + vector.y * matrix.m[1][0] + vector.z * matrix.m[2][0] + 1.0f * matrix.m[3][0];
	result.y = vector.x * matrix.m[0][1] + vector.y * matrix.m[1][1] + vector.z * matrix.m[2][1] + 1.0f * matrix.m[3][1];
	result.z = vector.x * matrix.m[0][2] + vector.y * matrix.m[1][2] + vector.z * matrix.m[2][2] + 1.0f * matrix.m[3][2];
	float w = vector.x * matrix.m[0][3] + vector.y * matrix.m[1][3] + vector.z * matrix.m[2][3] + 1.0f * matrix.m[3][3];
	assert(w != 0.0f);
	result.x /= w;
	result.y /= w;
	result.z /= w;
	return result;
}

Vector3 TransformNormal(const Vector3& v, const Matrix4x4& m) {
	Vector3 result{ v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0], v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1], v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] };
	return result;
}


//=====================================10.回転行列=====================================//
// 1. X軸回転行列
Matrix4x4 MakeRotateMatrixX(float radian) {
	Matrix4x4 result;
	result.m[0][0] = 1;
	result.m[0][1] = 0;
	result.m[0][2] = 0;
	result.m[0][3] = 0;

	result.m[1][0] = 0;
	result.m[1][1] = std::cosf(radian);
	result.m[1][2] = std::sinf(radian);
	result.m[1][3] = 0;

	result.m[2][0] = 0;
	result.m[2][1] = -std::sinf(radian);
	result.m[2][2] = std::cosf(radian);
	result.m[2][3] = 0;

	result.m[3][0] = 0;
	result.m[3][1] = 0;
	result.m[3][2] = 0;
	result.m[3][3] = 1;

	return result;
};
// 2. Y軸回転行列
Matrix4x4 MakeRotateMatrixY(float radian) {
	Matrix4x4 result;
	result.m[0][0] = std::cosf(radian);
	result.m[0][1] = 0;
	result.m[0][2] = -std::sinf(radian);
	result.m[0][3] = 0;

	result.m[1][0] = 0;
	result.m[1][1] = 1;
	result.m[1][2] = 0;
	result.m[1][3] = 0;

	result.m[2][0] = std::sinf(radian);
	result.m[2][1] = 0;
	result.m[2][2] = std::cosf(radian);
	result.m[2][3] = 0;

	result.m[3][0] = 0;
	result.m[3][1] = 0;
	result.m[3][2] = 0;
	result.m[3][3] = 1;

	return result;
};
// 3. Z軸回転行列
Matrix4x4 MakeRotateMatrixZ(float radian) {
	Matrix4x4 result;
	result.m[0][0] = std::cosf(radian);
	result.m[0][1] = std::sinf(radian);
	result.m[0][2] = 0;
	result.m[0][3] = 0;

	result.m[1][0] = -std::sinf(radian);
	result.m[1][1] = std::cosf(radian);
	result.m[1][2] = 0;
	result.m[1][3] = 0;

	result.m[2][0] = 0;
	result.m[2][1] = 0;
	result.m[2][2] = 1;
	result.m[2][3] = 0;

	result.m[3][0] = 0;
	result.m[3][1] = 0;
	result.m[3][2] = 0;
	result.m[3][3] = 1;

	return result;
}
Matrix4x4 MakeRotateMatrixXYZ(Vector3 rad)
{

	return Multiply(Multiply(MakeRotateMatrixX(rad.x), MakeRotateMatrixY(rad.y)), MakeRotateMatrixZ(rad.z));
};

//=============================11. 3次元のアフィン変換行列=============================//
Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate) {
	Matrix4x4 result = MakeIdentity4x4();

	Matrix4x4 matScale = MakeScaleMatrix(scale);
	Matrix4x4 matRotate = MakeRotateMatrixXYZ(rotate);
	Matrix4x4 matTrans = MakeTranslateMatrix(translate);

	result = matScale * matRotate * matTrans;
	return result;
	//return result;
}

//=============================12. レンタリングパイプラインVer2=============================//
// 1. 透視投影行列
Matrix4x4 MakePerspectiveFovMatrix(float FovY, float aspectRatio, float nearClip, float farClip) {
	Matrix4x4 result;
	result.m[0][0] = 1 / aspectRatio * (1 / std::tanf(FovY / 2));
	result.m[0][1] = 0;
	result.m[0][2] = 0;
	result.m[0][3] = 0;

	result.m[1][0] = 0;
	result.m[1][1] = (1 / std::tanf(FovY / 2));
	result.m[1][2] = 0;
	result.m[1][3] = 0;

	result.m[2][0] = 0;
	result.m[2][1] = 0;
	result.m[2][2] = farClip / (farClip - nearClip);
	result.m[2][3] = 1;

	result.m[3][0] = 0;
	result.m[3][1] = 0;
	result.m[3][2] = -nearClip * farClip / (farClip - nearClip);
	result.m[3][3] = 0;

	return result;
};

//Matrix4x4 MakePerspectiveFovMatrix(float FovY, float aspectRatio, float nearClip, float farClip) {
//	Matrix4x4 result;
//
//	// 縦方向の視野角に基づいてスケールを設定
//	float yScale = 1 / std::tanf(FovY / 2);
//	float xScale = yScale / aspectRatio;
//
//	// 行列の各要素を設定
//	result.m[0][0] = xScale;
//	result.m[0][1] = 0;
//	result.m[0][2] = 0;
//	result.m[0][3] = 0;
//
//	result.m[1][0] = 0;
//	result.m[1][1] = yScale;
//	result.m[1][2] = 0;
//	result.m[1][3] = 0;
//
//	result.m[2][0] = 0;
//	result.m[2][1] = 0;
//	result.m[2][2] = farClip / (farClip - nearClip);
//	result.m[2][3] = 1;
//
//	result.m[3][0] = 0;
//	result.m[3][1] = 0;
//	result.m[3][2] = -nearClip * farClip / (farClip - nearClip);
//	result.m[3][3] = 0;
//
//	return result;
//}

// 2. 正射影行列
Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip) {
	Matrix4x4 result;
	result.m[0][0] = 2 / (right - left);
	result.m[0][1] = 0;
	result.m[0][2] = 0;
	result.m[0][3] = 0;

	result.m[1][0] = 0;
	result.m[1][1] = 2 / (top - bottom);
	result.m[1][2] = 0;
	result.m[1][3] = 0;

	result.m[2][0] = 0;
	result.m[2][1] = 0;
	result.m[2][2] = 1 / (farClip - nearClip);
	result.m[2][3] = 0;

	result.m[3][0] = (left + right) / (left - right);
	result.m[3][1] = (top + bottom) / (bottom - top);
	result.m[3][2] = nearClip / (nearClip - farClip);
	result.m[3][3] = 1;

	return result;
};

Matrix4x4 MakeOrthographicMatrix_s(float left, float top, float right, float bottom, float nearClip, float farClip)
{
	Matrix4x4 result = {};

	result.m[0][0] = 2.0f / (right - left);
	result.m[1][1] = 2.0f / (top - bottom);
	result.m[2][2] = 1.0f / (farClip - nearClip);

	result.m[3][0] = (left + right) / (left - right);
	result.m[3][1] = (top + bottom) / (bottom - top);
	result.m[3][2] = nearClip / (nearClip- farClip);
	result.m[3][3] = 1.0f;

	return result;
}

// 3. ビューポート変換行列
Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth) {
	Matrix4x4 result;
	result.m[0][0] = width / 2;
	result.m[0][1] = 0;
	result.m[0][2] = 0;
	result.m[0][3] = 0;

	result.m[1][0] = 0;
	result.m[1][1] = -height / 2;
	result.m[1][2] = 0;
	result.m[1][3] = 0;

	result.m[2][0] = 0;
	result.m[2][1] = 0;
	result.m[2][2] = maxDepth - minDepth;
	result.m[2][3] = 0;

	result.m[3][0] = left + (width / 2);
	result.m[3][1] = top + (height / 2);
	result.m[3][2] = minDepth;
	result.m[3][3] = 1;

	return result;
};

Matrix4x4 ScaleMatrixFromVector3(const Vector3& scale)
{
	Matrix4x4 scaleMatrix = {};
	scaleMatrix.m[0][0] = scale.x;  // X方向のスケール
	scaleMatrix.m[1][1] = scale.y;  // Y方向のスケール
	scaleMatrix.m[2][2] = scale.z;  // Z方向のスケール
	scaleMatrix.m[3][3] = 1.0f;     // W成分は1.0

	return scaleMatrix;
}

Matrix4x4 TranslationMatrixFromVector3(const Vector3& translate)
{
	Matrix4x4 translationMatrix = {};
	translationMatrix.m[0][0] = 1.0f;  // 単位行列の成分
	translationMatrix.m[1][1] = 1.0f;  // 単位行列の成分
	translationMatrix.m[2][2] = 1.0f;  // 単位行列の成分
	translationMatrix.m[3][3] = 1.0f;  // 単位行列の成分

	translationMatrix.m[3][0] = translate.x;  // X方向の移動
	translationMatrix.m[3][1] = translate.y;  // Y方向の移動
	translationMatrix.m[3][2] = translate.z;  // Z方向の移動

	return translationMatrix;
}

Vector3 ExtractTranslation(const Matrix4x4& matrix)
{
	return Vector3(matrix.m[3][0], matrix.m[3][1], matrix.m[3][2]);
}

Vector3 MatrixToEuler(const Matrix4x4& m) {
	Vector3 euler;

	// Y軸の角度を計算（より安定した判定のために小さな閾値を使用）
	float threshold = 0.9999f; // ジンバルロックの判定閾値

	if (fabs(m.m[0][2]) < threshold) {
		// 通常のケース
		euler.y = std::asin(-m.m[0][2]);
		euler.x = std::atan2(m.m[1][2], m.m[2][2]);
		euler.z = std::atan2(m.m[0][1], m.m[0][0]);
	} else {
		// ジンバルロック状態
		float sign = (m.m[0][2] > 0) ? -1.0f : 1.0f;
		euler.y = sign * std::numbers::pi_v<float> / 2.0f;

		// XとZの自由度が1つになるため、任意に1つを固定することが一般的
		euler.z = 0.0f;
		euler.x = std::atan2(-sign * m.m[1][0], sign * m.m[1][1]);
	}

	return euler;
}

DirectX::XMMATRIX ConvertToXMMATRIX(const Matrix4x4& matrix)
{
	DirectX::XMMATRIX result;

	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			result.r[i].m128_f32[j] = matrix.m[i][j];
		}
	}

	return result;
}

Matrix4x4 MatrixLookAtLH(const Vector3& eye, const Vector3& target, const Vector3& up)
{
	Vector3 z_axis = { target.x - eye.x, target.y - eye.y, target.z - eye.z };
	z_axis = Normalize(z_axis);
	Vector3 x_axis = Cross(up, z_axis);

	// upとz_axisが平行だとCross結果がゼロベクトルになる
	const float Epsilon = 1.0e-6f;
	if (Dot(x_axis, x_axis) < Epsilon)
	{
		x_axis = { 1.0f, 0.0f, 0.0f };
	}
	x_axis = Normalize(x_axis);
	Vector3 y_axis = Cross(z_axis, x_axis);

	// Y軸がゼロベクトルになる可能性があるかをチェック
	if (Dot(y_axis, y_axis) < Epsilon)
	{
		// Z軸とX軸が何らかの理由で平行になった場合
		y_axis = { 0.0f, 1.0f, 0.0f };
	}

	float tx = -Dot(x_axis, eye);
	float ty = -Dot(y_axis, eye);
	float tz = -Dot(z_axis, eye);

	Matrix4x4 result = MakeIdentity4x4();

	// 行列の1行目 (X軸)
	result.m[0][0] = x_axis.x;
	result.m[0][1] = y_axis.x;
	result.m[0][2] = z_axis.x;
	result.m[0][3] = 0.0f;

	// 行列の2行目 (Y軸)
	result.m[1][0] = x_axis.y;
	result.m[1][1] = y_axis.y;
	result.m[1][2] = z_axis.y;
	result.m[1][3] = 0.0f;

	// 行列の3行目 (Z軸)
	result.m[2][0] = x_axis.z;
	result.m[2][1] = y_axis.z;
	result.m[2][2] = z_axis.z;
	result.m[2][3] = 0.0f;

	// 行列の4行目 (並進)
	result.m[3][0] = tx;
	result.m[3][1] = ty;
	result.m[3][2] = tz;
	result.m[3][3] = 1.0f;

	return result;
}

Matrix4x4 MakeUVTransform(const Vector2& offset, const Vector2& scale)
{
	// すべて 0 で初期化、または単位行列を作成
	Matrix4x4 mat = MakeIdentity4x4();

	// 1. スケーリング (UとVの密度)
	mat.m[0][0] = scale.x;
	mat.m[1][1] = scale.y;

	// 2. 平行移動 (UとVのスクロール量)
	mat.m[3][0] = offset.x;
	mat.m[3][1] = offset.y;

	return mat;
}




