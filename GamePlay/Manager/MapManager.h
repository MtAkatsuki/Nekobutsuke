#pragma once
#include    <functional>
#include	<memory>
#include	"../../System/CShader.h"
#include    "../../System/CIndexBuffer.h"
#include    "../../System/CVertexBuffer.h"
#include	"../../System/CMaterial.h"
#include	"../../System/CTexture.h"
#include	"../../System/CPlane.h"
#include    "../../System/collision.h"
#include	"../../System/DirectWrite.h"
#include	"../../System/Camera.h"
#include    "../../Actor/Gimmick/Trap.h"	
#include	"../../Actor/Gimmick/Prop.h"

class GameScene;
class GameContext;
class MapObject;

enum class TileType {
	FLOOR,
	WALL,
	TRAP
};

struct Tile {
	TileType type;
	int gridX, gridZ;
	bool isWalkable;
	class Unit* occupant = nullptr;

	// 静的オブジェクトレイヤー (壁、トラップ等のマップ構造物)
	class MapObject* structure = nullptr;

	Tile() : type(TileType::FLOOR), gridX(0), gridZ(0), isWalkable(true), occupant(nullptr) {}
};

// =========================================================
// MapManager クラス
// グリッド空間の管理、経路探索（BFS）、およびレベル（CSV）からの
// オブジェクト生成（Spawning）を一手に担う空間マネージャー。
// =========================================================
class MapManager {
public:
	MapManager() : m_mapWidth(30), m_mapDepth(30), m_tileSize(1.0f) {}

	// ---------------------------------------------------------
	// ライフサイクルと初期化
	// ---------------------------------------------------------
	void Init(GameContext* context);
	void LoadLevel(const std::string& csvPath, GameContext* context);
	void SetScene(GameScene* scene) { m_scene = scene; }

	// ---------------------------------------------------------
	// 空間クエリ・属性取得
	// ---------------------------------------------------------
	// 境界内かつ占有者・通行不可の構造物が無ければ true
	bool IsWalkable(int gridX, int gridZ) const;
	Vector3 GetWorldPosition(int gridX, int gridZ) const;
	Vector3 GetWorldPosition(const Tile& tile) const;

	const Tile* GetTile(int gridX, int gridZ) const;
	Tile* GetTile(int gridX, int gridZ);
	const std::vector<Tile>& GetAllTiles() const;

	int GetMapWidth() const { return m_mapWidth; }
	int GetMapDepth() const { return m_mapDepth; }

	// ワールド座標→グリッド添字。マップ内なら true
	bool WorldToGrid(const Vector3& world, int& gx, int& gz) const;
	// 円（半径 radius・中心 center）を近傍セルの壁 AABB から押し出した補正位置を返す
	Vector3 ResolveCircleCollision(const Vector3& center, float radius) const;

	// 円（center・radius）が近傍セルの壁 AABB に食い込むか
	bool CircleHitsWall(const Vector3& center, float radius) const;

	// ---------------------------------------------------------
	// アルゴリズム・経路探索
	// ---------------------------------------------------------
	int CalculateDistance(int x1, int z1, int x2, int z2) const { return std::abs(x1 - x2) + std::abs(z1 - z2); }
	// BFS で経路を返す。開始・目標マス自体は含まず、目標の手前で停止する。
	// ignoreTraps=true は罠を通行可能扱いにする（プレイヤー用）
	std::vector<Tile*> FindPaths(int startX, int startZ, int goalX, int goalZ, bool ignoreTraps = false);
	// maxSteps 歩以内に到達可能なマスを BFS で返す（開始マスも含む）
	std::vector<Tile*> GetReachableTiles(int startX, int startZ, int maxSteps);

	// ---------------------------------------------------------
	// 描画・状態操作
	// ---------------------------------------------------------
	void DrawColoredTiles(const std::vector<Tile*>& tiles, const DirectX::SimpleMath::Color& color);
	void ClearOccupants();

	// ---------------------------------------------------------
	// カメラ検査
	// ---------------------------------------------------------

	// カメラ衝突：from から dir 方向へ maxDist まで、最初の障害物までの距離（無ければ maxDist）
	float ProbeCameraObstacle(const Vector3& from, const Vector3& dir, float maxDist) const;
	// カメラ遮蔽判定：from から dir 方向へ maxDist まで走査し、途中の通行不可オブジェクトを out に追加（重複を除外）
	void CollectOccluders(const Vector3& from, const Vector3& dir, float maxDist,
		std::vector<class MapObject*>& out) const;
private:
	// =========================================================
	// レベル生成サブサブルーチン (Cataloging)
	// =========================================================
	std::vector<std::vector<std::string>> ParseCSV(const std::string& filePath);

	void ClearCurrentLevel();
	// CSV の行数・列数からグリッド寸法を確定し、全タイルを初期化する
	void SetupGridDimensions(const std::vector<std::vector<std::string>>& csvData);
	// 床レイヤーを敷き詰める
	void SpawnFloorLayer(GameContext* context);
	// 壁・罠・家具などの静的構造物を CSV から生成する
	void SpawnStaticStructures(const std::vector<std::vector<std::string>>& csvData, GameContext* context);
	// プレイヤー・敵・味方などの動的ユニットを CSV から生成する（床の後に追加）
	void SpawnDynamicEntities(const std::vector<std::vector<std::string>>& csvData, GameContext* context);

	const Tile* GetTileAtWorld(const Vector3& world) const;

	// 移動衝突用：そのセルが壁・通行不可家具か（occupant は見ない＝ユニットは素通り）
	bool IsBlockedForCollision(int gx, int gz) const;

private:
	int m_mapWidth;
	int m_mapDepth;
	float m_tileSize;
	std::vector<Tile> m_grid;
	Vector3 m_tileOffsets;

	std::vector<std::unique_ptr<MapObject>> m_mapObjects; // メモリ管理用コンテナ

	GameScene* m_scene = nullptr;
	GameContext* m_context = nullptr;

	class CStaticMeshRenderer* m_tileRenderer = nullptr;
	CStaticMeshRenderer* m_rangeRenderer = nullptr;
};