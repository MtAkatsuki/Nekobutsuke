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
	void SetScene(GameScene* scene) { m_Scene = scene; }

	// ---------------------------------------------------------
	// 空間クエリ・属性取得
	// ---------------------------------------------------------
	bool IsWalkable(int gridX, int gridZ) const;
	Vector3 GetWorldPosition(int gridX, int gridZ) const;
	Vector3 GetWorldPosition(const Tile& tile) const;

	const Tile* GetTile(int gridX, int gridZ) const;
	Tile* GetTile(int gridX, int gridZ);
	const std::vector<Tile>& GetAllTiles() const;

	int GetMapWidth() const { return m_mapWidth; }
	int GetMapDepth() const { return m_mapDepth; }

	// ---------------------------------------------------------
	// アルゴリズム・経路探索
	// ---------------------------------------------------------
	int CalculateDistance(int x1, int z1, int x2, int z2) const { return std::abs(x1 - x2) + std::abs(z1 - z2); }
	std::vector<Tile*> FindPaths(int startX, int startZ, int goalX, int goalZ, bool ignoreTraps = false);
	std::vector<Tile*> GetReachableTiles(int startX, int startZ, int maxSteps);

	// ---------------------------------------------------------
	// 描画・状態操作
	// ---------------------------------------------------------
	void DrawColoredTiles(const std::vector<Tile*>& tiles, const DirectX::SimpleMath::Color& color);
	void ClearOccupants();

private:
	// =========================================================
	// レベル生成サブサブルーチン (Cataloging)
	// =========================================================
	std::vector<std::vector<std::string>> ParseCSV(const std::string& filePath);

	void ClearCurrentLevel();
	void SetupGridDimensions(const std::vector<std::vector<std::string>>& csvData);
	void SpawnFloorLayer(GameContext* context);
	void SpawnStaticStructures(const std::vector<std::vector<std::string>>& csvData, GameContext* context);
	void SpawnDynamicEntities(const std::vector<std::vector<std::string>>& csvData, GameContext* context);

private:
	int m_mapWidth;
	int m_mapDepth;
	float m_tileSize;
	std::vector<Tile> m_grid;
	Vector3 m_tileOffsets;

	std::vector<std::unique_ptr<MapObject>> m_mapObjects; // メモリ管理用コンテナ

	GameScene* m_Scene = nullptr;
	GameContext* m_context = nullptr;

	class CStaticMeshRenderer* m_tileRenderer = nullptr;
	CStaticMeshRenderer* m_rangeRenderer = nullptr;
};