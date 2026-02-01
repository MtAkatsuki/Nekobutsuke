#pragma once
#include    <functional>
#include	<memory>
#include	"../system/CShader.h"
#include    "../system/CIndexBuffer.h"
#include    "../system/CVertexBuffer.h"
#include	"../system/CMaterial.h"
#include	"../system/CTexture.h"
#include	"../system/CPlane.h"
#include    "../system/collision.h"
#include	"../system/DirectWrite.h"
#include	"../system/camera.h"
#include    "../gameobject//Trap.h"	
#include	"../gameobject/Prop.h"

class GameScene;
class GameContext;

enum class TileType {
	FLOOR,
	WALL,
	TRAP
};

struct Tile {
	TileType type;
	int gridX, gridZ;// グリッド上の位置
	bool isWalkable;
	class Unit* occupant =nullptr;

	// 静的オブジェクトレイヤー (壁、トラップ等) ---
	// キャラクターとは異なり、座標が固定されているマップオブジェクトを保持する
	class MapObject* structure = nullptr;

	Tile() :type(TileType::FLOOR), gridX(0), gridZ(0), isWalkable(true), occupant(nullptr) {}
};

class MapManager {

public:
	MapManager() :m_mapWidth(30), m_mapDepth(30), m_tileSize(1) {}
	void Init(GameContext* context);
	//通過できるか
	bool IsWalkable(int gridX, int gridZ) const;
	//ステージロード用
	void LoadLevel(const std::string& csvPath, GameContext* context);

	Vector3 GetWorldPosition(int gridX, int gridZ) const;
	Vector3 GetWorldPosition(const Tile& tile) const;

	const Tile* GetTile (int gridX, int gridZ) const;
	Tile* GetTile(int gridX, int gridZ);

	const std::vector<Tile>& GetAllTiles() const;

	int GetMapWidth() const { return m_mapWidth; }
	int GetMapDepth() const { return m_mapDepth; }

	void SetScene(GameScene* scene) { m_Scene = scene;}
	//マンハッタン距離計算
	int CalculateDistance(int x1, int z1, int x2, int z2)const { return std::abs(x1 - x2) + std::abs(z1 - z2); }
	//幅優先探索breadth first search
	std::vector<Tile*> FindPaths(int startX, int startZ, int goalX,int goalZ);
	//BFSで到達可能なタイルを取得
	std::vector<Tile*> GetReachableTiles(int startX, int startZ, int maxSteps);
	//移動範囲のタイルを色付きで描画
	void DrawColoredTiles(const std::vector<Tile*>& tiles, const DirectX::SimpleMath::Color& color);
	//全グリッド上のユニット参照をクリア
	void ClearOccupants();
private:
	// --- CSV解析用のヘルパー関数 ---
	// 指定されたパスのCSVファイルを読み込み、文字列の2次元配列として返す
	std::vector<std::vector<std::string>> ParseCSV(const std::string& filePath);

	// --- 全てのMapObjectのメモリ管理用コンテナ ---
	// unique_ptrを使用することで、マップ破棄時にオブジェクトも自動的に解放される
	std::vector<std::unique_ptr<MapObject>> m_mapObjects;
private:
	int m_mapWidth;
	int m_mapDepth;
	float m_tileSize;
	std::vector<Tile> m_grid;
	Vector3 m_tileOffsets;
	GameScene* m_Scene = nullptr;
	GameContext* m_context = nullptr;
	// タイル描画用メッシュレンダラー(効率化)
	class CStaticMeshRenderer* m_tileRenderer = nullptr;
	CStaticMeshRenderer* m_rangeRenderer = nullptr;
};