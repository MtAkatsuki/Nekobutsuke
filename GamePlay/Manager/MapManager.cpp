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

//通過できるかの検査
bool MapManager::IsWalkable(int gridX, int gridZ) const
{
	const Tile* targetTile = GetTile(gridX, gridZ);

	if (targetTile == nullptr) { return false; }//マップの境界外
	//Unitの占有検査
	if (targetTile->occupant != nullptr) {
		return false;
	}

	//  静的オブジェクト（障害物・構造物）のチェック
	// ターゲットとなるタイルに壁やオブジェクトが存在するかを確認
	if (targetTile->structure != nullptr) {

		// オブジェクトの通行可能フラグ（IsWalkable）を確認
		if (!targetTile->structure->IsWalkable()) {
			// 壁などの通行不可能な構造物がある場合、移動不可（false）を返す
			return false;
		}
	}
	return true;
}

std::vector<Tile*> MapManager::FindPaths(int startX, int startZ, int goalX, int goalZ, bool ignoreTraps)
{
	//境界とスタットポイントの検査
	Tile* startTile = GetTile(startX, startZ);
	Tile* goalTile = GetTile(goalX, goalZ);

	if (!startTile || !goalTile) { return {}; }//無効なスタートまたはゴール,GetTileがnullptrを返す
	if (startTile == goalTile) { return {}; }//スタートとゴールが同じ、移動しない

	//幅優先探索ときのTileを保存するキュー
	std::queue<Tile*> frontier;
	//スタットポイントをキューに追加(for循環最初の目標)
	frontier.push(startTile);

	//Tile を追跡するマップ,key=currentTile,value=前のTile
	std::unordered_map<Tile*, Tile*> cameFrom;
	cameFrom[startTile] = nullptr; //スタットポイントの前のTileはなし

	//行き止まりのとき一番近いTileを移動
	//探索中で見つかったゴールに一番近いTileを保存
	Tile* bestTile = startTile;
	int minDistance = CalculateDistance(startX, startZ, goalX, goalZ);

	bool foundPath = false;

	//BFS探索ループ
	while (!frontier.empty())
	{
		Tile* currentTile = frontier.front();//キューの先頭を取得
		frontier.pop();//キューの先頭を削除(探索したTileを削除)

		//currentからgoalまでの距離を計算
		int currentDistance = CalculateDistance(currentTile->gridX, currentTile->gridZ, goalX, goalZ);

		//もしcurrentTileは前のbestTileより近いなら、bestTileを更新
		if (currentDistance < minDistance)
		{
			minDistance = currentDistance;//一番近い距離を今の距離に更新、次はまた新しいのcurrentDistanceを比較
			bestTile = currentTile;//bestTileを今到達したcurrentTileに更新
		}
		//ゴールに到達したら探索終了
		if (currentTile == goalTile)
		{
			foundPath = true;
			bestTile = currentTile;//bestTileをゴールに設定(最後に見つかったTile)
			break;
		}

		//隣接するタイルを調査(上下左右)
		int dx[] = { -1, 1, 0, 0 };
		int dz[] = { 0, 0, -1, 1 };

		for (int i = 0; i < 4; ++i)
		{
			int nextX = currentTile->gridX + dx[i];
			int nextZ = currentTile->gridZ + dz[i];
			Tile* nextTile = GetTile(nextX, nextZ);
			//GetTileがnullptrを返すか、通過できないTile、すでに調査したTileはスキップ(came_from.count == 0)
			if (nextTile && cameFrom.find(nextTile) == cameFrom.end())
			{
				//地形検査
				bool isWalkable = IsWalkable(nextX, nextZ);
				if (!isWalkable) { continue; }//通過できないならスキップ

				// AI専用ロジックをパラメータで制御
				// ignoreTraps が false（AIの場合）：TRAP（トラップ）を進入不可（通行禁止）として扱う
				// ignoreTraps が true（プレイヤーの場合）：トラップを無視して通行可能とする
				if (!ignoreTraps) {
					if (nextTile->structure && nextTile->structure->GetType() == MapModelType::TRAP) {
						// トラップがあるタイルはスキップ（経路候補から除外）
						continue;
					}
				}


				//Unit検査
				if (nextTile->occupant != nullptr)
				{
					//特殊ケース:ゴールTileは通過できなくても追加、後でゴールの隣接Tileを取得するために
					if (nextTile != goalTile) { continue; }
				}
				frontier.push(nextTile);//ゴールでもキューに追加
				cameFrom[nextTile] = currentTile;//前のTileを記録
			}
		}
	}

	//foundPathがfalseの場合、もしくはfoundPathがtrueの場合、どっちもbestTile(すなわち取得できる一番ゴールに近いTile)からパスを再構築
	//もしfoundPathがtrueの場合、bestTileはgoalTileになる
	//もしfoundPathがfalseの場合、bestTileはスタットポイントに一番近いTileになる
	std::vector<Tile*> path;
	Tile* currentTile = bestTile;

	//実行できるゴールがある時、cameFromに保存されたTileをpathに追加
	while (currentTile != startTile)
	{
		path.push_back(currentTile);//unordered_mapは順番がないので、vectorに入れて後でreverseする
		currentTile = cameFrom[currentTile];//元のcurrentTileをpathに入れた後、前のTileに更新、ループで繰り返し
	}

	std::reverse(path.begin(), path.end());//pathを逆順にする、スタットポイントからゴールまでの順番に

	//最後はゴールTileの隣接Tileを取得するために、ゴールTileをpathから削除
	if (foundPath && !path.empty())
	{
		if (path.back() == goalTile)
		{
			path.pop_back();//ゴールTileをpathから削除、移動先として含めない
		}
	}

	return path;
}

//移動範囲のタイルを取得(BFS)-スタート点の引数を渡す
std::vector<Tile*> MapManager::GetReachableTiles(int startX, int startZ, int maxSteps)
{
	std::vector<Tile*> reachables;
	Tile* startTile = GetTile(startX, startZ);
	if (!startTile) return reachables;

	//データ構造初期化およびスタートタイルの追加
	//BFS用のキュー:<Tile*,int>ペア、intはスタートからのステップ数
	std::queue<std::pair<Tile*, int>> q;//FIFO
	q.push({ startTile, 0 });

	//訪問済みタイルを追跡するセット（unordered_setでO(1)判定）
	std::unordered_set<Tile*> visited;
	visited.insert(startTile);
	reachables.push_back(startTile);//スタートタイルも移動範囲内に含む

	while (!q.empty()) //キューが空になるまでループ
	{
		auto [current, dist] = q.front();//キューの先頭を取得,current=Tile*,dist=int
		q.pop();//キューの先頭を削除

		//最大ステップ数に達したらスキップ、以降の隣接タイルは追加しない
		// (Tile自身はすでに前回のループで追加された)
		//次のq.front()実行する
		if (dist >= maxSteps) continue;

		int dx[] = { -1, 1, 0, 0 };
		int dz[] = { 0, 0, -1, 1 };

		for (int i = 0; i < 4; ++i)
		{
			int nx = current->gridX + dx[i];
			int nz = current->gridZ + dz[i];
			Tile* next = GetTile(nx, nz);

			if (next && IsWalkable(nx, nz))
			{
				// insert の戻り値で「未訪問なら追加」を一度に判定（O(1)）
				if (visited.insert(next).second)
				{
					reachables.push_back(next);
					q.push({ next, dist + 1 });
				}
			}
		}
	}
	return reachables;
}

//移動範囲のタイルを色付きで描画
void MapManager::DrawColoredTiles(const std::vector<Tile*>& tiles, const DirectX::SimpleMath::Color& color)
{
	if (tiles.empty() || !m_rangeRenderer) { return; }
	//マテリアル取得
	CMaterial* mtrl = m_rangeRenderer->GetMaterial(0);
	if (!mtrl) { return; }
	//元のマテリアルデータを保存
	MATERIAL original = mtrl->GetData();

	//純色半透明のマテリアルを作成
	MATERIAL temp = original;
	temp.Diffuse = color;
	temp.Ambient = color;//環境ライトは同じ色に、明るさの確保
	temp.TextureEnable = FALSE;//テクスチャ無効化、色だけで描画
	mtrl->SetMaterial(temp);

	//各タイルを描画
	for (Tile* t : tiles)
	{
		Vector3 pos = GetWorldPosition(t->gridX, t->gridZ);
		pos.y += ZFight::RangePanel; //少し浮かせて描画

		//見やすいために少し小さくする
		Matrix4x4 world = Matrix4x4::CreateScale(RANGE_TILE_SCALE) * Matrix4x4::CreateTranslation(pos);
		Renderer::SetWorldMatrix(&world);
		m_rangeRenderer->Draw();
	}

	//元のマテリアルに戻す
	mtrl->SetMaterial(original);
}

void MapManager::ClearOccupants()
{
	// 全グリッドを走査し、占有者（ユニット参照）をクリアする。
	for (auto& tile : m_grid) {
		tile.occupant = nullptr;
	}
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
		m_grid[i].occupant = nullptr;
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
				GetTile(x, z)->occupant = pPlayer;
			}
			else if (token == "A") {
				auto ally = Ally::Spawn(context, x, z, worldPos);
				GetTile(x, z)->occupant = ally.get();
				context->SetAlly(ally.get());
				unitsToSpawn.push_back(std::move(ally));
			}
			else if (token[0] == 'E') {

				auto enemy = Enemy::Spawn(context, x, z, worldPos);
				GetTile(x, z)->occupant = enemy.get();

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