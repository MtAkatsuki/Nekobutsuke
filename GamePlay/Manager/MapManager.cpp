#include <memory>
#include <queue>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <cmath>
#include "MapManager.h"
#include "EnemyManager.h"
#include "../../Core/DebugLog.h"
#include "../../Actor/Base/GameObject.h"
#include "../../GamePlay/Scene/GameScene.h"
#include "../../Actor/Base/MapObject.h"
#include "../../System/Utility/CSVParser.h"
#include "../../System/RandomEngine.h"
#include "../../System/Utility/WorldToScreen.h"
#include "../../System/MeshManager.h"
#include "../../System/ZFightTunables.h"
#include "../../Actor/Character/Player.h"
#include "../../Actor/Character/Enemy.h"
#include "../../Actor/Character/Ally.h"
#include "../../Core/GameContext.h"

namespace {
	// 視覚的オフセット（Zファイティング防止・パースペクティブ調整用）
	const float VISUAL_Z_OFFSET = -0.3f;

	// マップ構成の既定値（LoadLevel が CSV から上書きするまでの初期値）
	const int DEFAULT_MAP_WIDTH = 12;
	const int DEFAULT_MAP_DEPTH = 9;
	const float TILE_SIZE = 1.0f;          // 1タイルのワールド空間サイズ

	const float RANGE_TILE_SCALE = 0.9f;   // 範囲表示パネルの縮小率（境界を見やすくする）
}

void MapManager::Init(GameContext* context) {
	m_mapWidth = DEFAULT_MAP_WIDTH;
	m_mapDepth = DEFAULT_MAP_DEPTH;
	m_tileSize = TILE_SIZE;
	m_tileOffsets.x = 0.5f * -(float(GetMapWidth()) * m_tileSize);
	m_tileOffsets.y = 0.0f;
	m_tileOffsets.z = 0.5f * -(float(GetMapDepth()) * m_tileSize);

	// グリッド初期化
	m_grid.clear();
	m_grid.resize(m_mapWidth * m_mapDepth);//一気にメモリーを確保


	m_tileRenderer = MeshManager::GetRenderer<CStaticMeshRenderer>("floor_mesh");
	m_rangeRenderer = MeshManager::GetRenderer<CStaticMeshRenderer>("range_panel_mesh");
}

//Tile対象作らない、メモリーに効率がいい
Vector3 MapManager::GetWorldPosition(int gridX, int gridZ) const
{
	//左下の座標を計算
	float worldX = (float)gridX * m_tileSize;
	float worldY = 0;
	float worldZ = (float)gridZ * m_tileSize;

	//タイルの中心に調整
	worldX = m_tileOffsets.x + worldX + (m_tileSize / 2.0f);
	worldZ = m_tileOffsets.z + worldZ + (m_tileSize / 2.0f);

	// Z軸の負の方向（カメラ方向）へオフセットさせ、物体をマスの手前側に寄せる
	worldZ += VISUAL_Z_OFFSET;

	return Vector3(worldX, worldY, worldZ);
}

//直接Tileを渡す方法、もっと分かりやすいサポート関数
Vector3 MapManager::GetWorldPosition(const Tile& tile) const
{
	return GetWorldPosition(tile.gridX, tile.gridZ);
}

//Tileの記号を取得する
const Tile* MapManager::GetTile(int gridX, int gridZ)const
{
	if (gridX < 0 || gridX >= m_mapWidth || gridZ < 0 || gridZ >= m_mapDepth) {
		return nullptr;
	}

	int index = gridZ * m_mapWidth + gridX;
	return &m_grid[index];

}

Tile* MapManager::GetTile(int gridX, int gridZ)
{
	if (gridX < 0 || gridX >= m_mapWidth || gridZ < 0 || gridZ >= m_mapDepth) {
		return nullptr;
	}

	int index = gridZ * m_mapWidth + gridX;
	return &m_grid[index];

}

// drawのため、すべてのタイルを取得
const std::vector<Tile>& MapManager::GetAllTiles()const
{
	return m_grid;
}



// ---------------------------------------------------------

// レベルロード・パイプライン (Level Loading Pipeline)

// ---------------------------------------------------------

void MapManager::LoadLevel(const std::string& csvPath, GameContext* context) {
	DBG_TRACE("[MapManager] Loading Level: " << csvPath);
	// 1. 古いデータの破棄
	ClearCurrentLevel();

	// 2. CSVの解析データを取得
	auto csvData = CSVParser::ReadAsGrid(csvPath);
	if (csvData.empty()) {
		DBG_ERROR("[MapManager] Level data is empty! Aborting Map generation.");
		return;
	}

	// 3. グリッド寸法の確立と初期化
	SetupGridDimensions(csvData);

	// 4. マップレイヤーの生成（Zオーダーの奥から手前へ）
	SpawnFloorLayer(context);
	SpawnStaticStructures(csvData, context);
	SpawnDynamicEntities(csvData, context);
	DBG_TRACE("[MapManager] Level Loaded Successfully.");
}

void MapManager::ClearCurrentLevel() {
	m_mapObjects.clear();
	m_grid.clear();
}

void MapManager::SetupGridDimensions(const std::vector<std::vector<std::string>>& csvData) {
	// csvData の要素数（外側の vector）は「行数」であり、マップの「奥行き（Z軸）」に対応する
	m_mapDepth = static_cast<int>(csvData.size());
	// csvData[0]（最初の行）の要素数は「列数」であり、マップの「幅（X軸）」に対応する
	m_mapWidth = static_cast<int>(csvData[0].size());

	m_tileSize = TILE_SIZE;
	m_tileOffsets.x = 0.5f * -(static_cast<float>(m_mapWidth) * m_tileSize);
	m_tileOffsets.y = 0.0f;
	m_tileOffsets.z = 0.5f * -(static_cast<float>(m_mapDepth) * m_tileSize);

	m_grid.resize(m_mapWidth * m_mapDepth);

	for (int i = 0; i < static_cast<int>(m_grid.size()); ++i) {
		m_grid[i].gridX = i % m_mapWidth;
		m_grid[i].gridZ = i / m_mapWidth;
		m_grid[i].type = TileType::FLOOR;
		m_grid[i].isWalkable = true;
		m_grid[i].structure = nullptr;
	}
}

void MapManager::SpawnFloorLayer(GameContext* context) {
	for (int z = 0; z < m_mapDepth; ++z) {
		for (int x = 0; x < m_mapWidth; ++x) {
			auto floorObj = std::make_unique<MapObject>(context);
			floorObj->Init(MapModelType::FLOOR, GetWorldPosition(x, z));
			if (m_scene) m_scene->AddObject(std::move(floorObj));
		}
	}
}

void MapManager::SpawnStaticStructures(const std::vector<std::vector<std::string>>& csvData, GameContext* context) {
	for (int z = 0; z < m_mapDepth; ++z) {
		int csvRowIndex = (m_mapDepth - 1) - z; // CSVのZ軸反転
		for (int x = 0; x < m_mapWidth; ++x) {
			if (x >= csvData[csvRowIndex].size()) continue;

			Tile* currentTile = GetTile(x, z);
			Vector3 worldPos = GetWorldPosition(x, z);

			// 重複生成の防止（大型オブジェクトの一部として既に処理されている場合）
			if (currentTile->structure != nullptr) continue;

			std::string token = csvData[csvRowIndex][x];
			// プロップ定義テーブルからトークンを解決（未知トークン＝床は生成しない）
			const PropDef* def = FindPropDefByToken(token);

			if (def) {
				MapModelType propType = def->type;
				int sizeX = def->sizeX, sizeZ = def->sizeZ;

				std::unique_ptr<MapObject> newObj;
				if (propType == MapModelType::TRAP) {
					newObj = std::make_unique<Trap>(context);
				}
				else {
					newObj = std::make_unique<Prop>(context);
				}

				float offsetX = (sizeX - 1) * m_tileSize * 0.5f;
				float offsetZ = (sizeZ - 1) * m_tileSize * 0.5f;
				Vector3 centerPos = worldPos;
				centerPos.x += offsetX;
				centerPos.z += offsetZ;

				newObj->Init(propType, centerPos);
				newObj->SetGridPosition(x, z);

				MapObject* rawPtr = newObj.get();
				// 占有タイルの登録
				for (int i = 0; i < sizeX; ++i) {
					for (int j = 0; j < sizeZ; ++j) {
						Tile* t = GetTile(x + i, z + j);
						if (t) t->structure = rawPtr;
					}
				}

				if (m_scene) m_scene->AddObject(std::move(newObj));
			}
		}
	}
}

void MapManager::SpawnDynamicEntities(const std::vector<std::vector<std::string>>& csvData, GameContext* context) {
	// ユニット（Player, Enemy, Ally）は、床（MapObject）が全て追加された「後」にSceneに追加する。
	 
	// これにより、GameSceneの描画ループで「床 -> ユニット」の順になり、
	
	// 残像（Ghost）やUIが床に隠れる問題（Z-Fighting/Occlusion）が解決する。
	std::vector<std::unique_ptr<GameObject>> unitsToSpawn;

	for (int z = 0; z < m_mapDepth; ++z) {
		int csvRowIndex = (m_mapDepth - 1) - z;
		for (int x = 0; x < m_mapWidth; ++x) {
			std::string token = csvData[csvRowIndex][x];
			Vector3 worldPos = GetWorldPosition(x, z);

			if (token == "P") {
				Player* pPlayer = context->GetPlayer();
				if (!pPlayer) {
					auto newPlayer = Player::Spawn(context, x, z, worldPos);
					pPlayer = newPlayer.get();
					context->SetPlayer(pPlayer);
					unitsToSpawn.push_back(std::move(newPlayer));
				}
				else {
					pPlayer->SetGridPosition(x, z); 
					pPlayer->SetPosition(worldPos);
					pPlayer->UpdateWorldMatrix();
				}
			}
			else if (token == "A") {
				auto ally = Ally::Spawn(context, x, z, worldPos);
				context->SetAlly(ally.get());
				unitsToSpawn.push_back(std::move(ally));
			}
			else if (token[0] == 'E') {

				auto enemy = Enemy::Spawn(context, x, z, worldPos);

				if (context->GetEnemyManager()) context->GetEnemyManager()->RegisterEnemy(enemy.get());
				unitsToSpawn.push_back(std::move(enemy));
			}
		}
	}

	if (context->GetEnemyManager()) {
		for (Enemy* e : context->GetEnemyManager()->GetAllEnemies()) {
			if (e) e->SetInitialFacingToPlayer();
		}
	}

	if (m_scene) {
		for (auto& unitObj : unitsToSpawn) {
			m_scene->AddObject(std::move(unitObj));
		}
	}
}

const Tile* MapManager::GetTileAtWorld(const Vector3& world) const {
	// GetWorldPosition の逆変換（Z は VISUAL_Z_OFFSET を戻す）
	float fx = (world.x - m_tileOffsets.x - m_tileSize * 0.5f) / m_tileSize;
	float fz = (world.z - m_tileOffsets.z - m_tileSize * 0.5f - VISUAL_Z_OFFSET) / m_tileSize;
	int gx = (int)std::floor(fx + 0.5f);
	int gz = (int)std::floor(fz + 0.5f);
	return GetTile(gx, gz);
}


void MapManager::CollectOccluders(const Vector3& from, const Vector3& dir, float maxDist,
	std::vector<MapObject*>& out) const {
	const float step = m_tileSize * 0.25f;   // サンプリング間隔（1マスの1/4）
	MapObject* last = nullptr;
	for (float d = step; d <= maxDist; d += step) {
		Vector3 p = from + dir * d;
		const Tile* t = GetTileAtWorld(p);
		if (!t) continue;
		MapObject* s = t->structure;
		// 通行不可の構造物（壁・家具）のみを遮蔽物として扱う。隣接サンプルの重複＋全体の重複を除外
		if (s && !s->IsWalkable() && s != last) {
			if (std::find(out.begin(), out.end(), s) == out.end()) out.push_back(s);
			last = s;
		}
	}
}

bool MapManager::WorldToGrid(const Vector3& world, int& gx, int& gz) const {
	// GetTileAtWorld と同じ逆変換（Z は VISUAL_Z_OFFSET を戻す）
	float fx = (world.x - m_tileOffsets.x - m_tileSize * 0.5f) / m_tileSize;
	float fz = (world.z - m_tileOffsets.z - m_tileSize * 0.5f - VISUAL_Z_OFFSET) / m_tileSize;
	gx = static_cast<int>(std::floor(fx + 0.5f));
	gz = static_cast<int>(std::floor(fz + 0.5f));
	return (gx >= 0 && gx < m_mapWidth && gz >= 0 && gz < m_mapDepth);
}

bool MapManager::IsBlockedForCollision(int gx, int gz) const {
	const Tile* t = GetTile(gx, gz);
	if (!t) return true;                                   // マップ外は壁扱い（場外へ出さない）
	if (t->structure && !t->structure->IsWalkable()) return true; // 壁・通行不可家具
	return false; 
}

Vector3 MapManager::ResolveCircleCollision(const Vector3& center, float radius) const {
	using namespace GM31::GE::Collision;
	Vector3 c = center;

	int gx, gz;
	WorldToGrid(c, gx, gz);

	// 円の中心セル周囲 3x3 の壁だけを対象に押し出す
	// （1フレームの移動量は 1 セル未満なので 3x3 で十分）
	for (int dz = -1; dz <= 1; ++dz) {
		for (int dx = -1; dx <= 1; ++dx) {
			int tx = gx + dx, tz = gz + dz;
			if (!IsBlockedForCollision(tx, tz)) continue;

			// セル中心から 1x1 の AABB を作る。XZ 判定なので Y は広く取り、
			// 「最近接点の Y = 円中心の Y」となって Y 成分が判定に混ざらないようにする
			Vector3 wc = GetWorldPosition(tx, tz);
			BoundingBoxAABB box;
			box.min = Vector3(wc.x - m_tileSize * 0.5f, c.y - 1000.0f, wc.z - m_tileSize * 0.5f);
			box.max = Vector3(wc.x + m_tileSize * 0.5f, c.y + 1000.0f, wc.z + m_tileSize * 0.5f);

			// AABB 上で円中心に最も近い点 q
			Vector3 q;
			ClosestPtPointAABB(c, box, q);

			Vector3 d = c - q;
			d.y = 0.0f;                       // 平面（XZ）でのめり込みだけ見る
			float dist = d.Length();
			if (dist < radius) {              // めり込んでいる
				if (dist > 0.0001f) {
					d /= dist;
					c += d * (radius - dist); // めり込み量だけ法線方向へ押し戻す
				}
				else {
					c.x += radius;            // 中心が壁内部の稀ケースは +X へ退避
				}
			}
		}
	}
	return c;
}

bool MapManager::CircleHitsWall(const Vector3& center, float radius) const {
	using namespace GM31::GE::Collision;
	int gx, gz;
	WorldToGrid(center, gx, gz);
	for (int dz = -1; dz <= 1; ++dz) {
		for (int dx = -1; dx <= 1; ++dx) {
			int tx = gx + dx, tz = gz + dz;
			if (!IsBlockedForCollision(tx, tz)) continue;
			Vector3 wc = GetWorldPosition(tx, tz);
			BoundingBoxAABB box;
			box.min = Vector3(wc.x - m_tileSize * 0.5f, center.y - 1000.0f, wc.z - m_tileSize * 0.5f);
			box.max = Vector3(wc.x + m_tileSize * 0.5f, center.y + 1000.0f, wc.z + m_tileSize * 0.5f);
			if (CollisionSphereAABB(BoundingSphere{ center, radius }, box)) return true;
		}
	}
	return false;
}

std::vector<std::pair<int, int>> MapManager::FindWallPath(int sx, int sz, int gx, int gz) const {
	std::vector<std::pair<int, int>> result;
	const int W = m_mapWidth, D = m_mapDepth;
	auto inBounds = [&](int x, int z) { return x >= 0 && x < W && z >= 0 && z < D; };
	if (!inBounds(sx, sz) || !inBounds(gx, gz)) return result;
	if (IsBlockedForCollision(gx, gz)) return result;   // goal が壁なら不可

	auto idx = [&](int x, int z) { return z * W + x; };
	std::vector<int> prev(W * D, -2);   // -2=未訪問, -1=始点
	std::queue<int> q;
	prev[idx(sx, sz)] = -1;
	q.push(idx(sx, sz));
	const int dx[] = { 1,-1,0,0 }, dz[] = { 0,0,1,-1 };
	bool found = false;

	while (!q.empty()) {
		int cur = q.front(); q.pop();
		int cx = cur % W, cz = cur / W;
		if (cx == gx && cz == gz) { found = true; break; }
		for (int i = 0; i < 4; ++i) {
			int nx = cx + dx[i], nz = cz + dz[i];
			if (!inBounds(nx, nz)) continue;
			if (prev[idx(nx, nz)] != -2) continue;
			if (IsBlockedForCollision(nx, nz)) continue;
			prev[idx(nx, nz)] = cur;
			q.push(idx(nx, nz));
		}
	}
	if (!found) return result;

	for (int cur = idx(gx, gz); cur != -1; cur = prev[cur])
		result.push_back({ cur % W, cur / W });
	std::reverse(result.begin(), result.end());   // goal→start を start→goal へ
	return result;
}