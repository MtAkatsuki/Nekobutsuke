#include <memory>
#include <queue>
#include <algorithm> // for reverse
#include	"../gameobject/GameObject.h"
#include	"../scene/GameScene.h"
#include	"../manager/MapManager.h"
#include	"../gameobject/MapObject.h"
#include	"../system/stb_perlin.h"
#include	"../system/RandomEngine.h"
#include	"../system/collision.h"
#include	"../utility/WorldToScreen.h"
#include    "../system/meshmanager.h"

void MapManager::Init(GameContext* context) {
	m_mapWidth = 9;
	m_mapDepth = 9;
	m_tileSize = 1.0f;
	m_tileOffsets.x = 0.5f * -(float(GetMapWidth()) * m_tileSize);
	m_tileOffsets.y = 0.0f;
	m_tileOffsets.z = 0.5f * -(float(GetMapDepth()) * m_tileSize);

	// グリッド初期化
	m_grid.clear();
	m_grid.resize(m_mapWidth * m_mapDepth);//一気にメモリーを確保
	//Tile初期化
	for (int z = 0; z < m_mapDepth; z++) 
	{
		for (int x = 0; x < m_mapWidth; x++) 
		{
			int index = z * m_mapWidth + x;//1次元配列のインデックス計算
			Tile& currentTile = m_grid[index];//参照取得
			// グリッド上の位置をセット
			currentTile.gridX = x;
			currentTile.gridZ = z;


			currentTile.type = TileType::FLOOR;
			currentTile.isWalkable = true;
			std::unique_ptr<MapObject> floorObj = std::make_unique<MapObject>(context);
			floorObj->Init(MapModelType::FLOOR, GetWorldPosition(x, z));
			m_Scene->AddObject(std::move(floorObj));
		
		}
	}

	m_tileRenderer = MeshManager::getRenderer<CStaticMeshRenderer>("floor_mesh");
	m_rangeRenderer = MeshManager::getRenderer<CStaticMeshRenderer>("range_panel_mesh");
}

// グリッド座標からワールド座標を取得
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

	return Vector3(worldX, worldY, worldZ);
}

//直接Tileを渡す方法、もっと分かりやすいサポート関数
Vector3 MapManager::GetWorldPosition(const Tile& tile) const
{
	return GetWorldPosition(tile.gridX, tile.gridZ);
}

//Tileの記号を取得する
const Tile* MapManager::GetTile (int gridX, int gridZ)const
{
	if(gridX < 0 || gridX >= m_mapWidth || gridZ < 0 || gridZ >= m_mapDepth) {
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

	if (targetTile->type == TileType::WALL) { return false; }//壁Tile

	if (targetTile->type == TileType::TRAP_HOLE) { return false; }//落とし穴Tile

	if (targetTile->occupant != nullptr) {
		return false;
	}

	return true;
}

std::vector<Tile*> MapManager::FindPaths(int startX, int startZ, int goalX,int goalZ)
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
		if(currentDistance < minDistance)
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

		for(int i=0;i<4;i++) 
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
				
				//Unit検査
				if (nextTile->occupant!=nullptr) 
				{	
					//特殊ケース:ゴールTileは通過できなくても追加、後でゴールの隣接Tileを取得するために
					if(nextTile!= goalTile)	{continue;}		
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

	//訪問済みタイルを追跡するセット
	std::vector<Tile*> visited;
	visited.push_back(startTile);
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

		for (int i = 0; i < 4; i++) 
		{
			int nx = current->gridX + dx[i];
			int nz = current->gridZ + dz[i];
			Tile* next = GetTile(nx, nz);

			if (next && IsWalkable(nx, nz))
			{
				bool isVisited = false;
				for (auto* v : visited) //訪問済みタイルの検査
				{
					if (v == next)
					{
						isVisited = true;
						break;
					}
				}
				if (!isVisited) 
				{
					visited.push_back(next);
					reachables.push_back(next);
					q.push({ next,dist + 1 });//ここで次検査する必要なTileをキューに追加(1->4->4*3->...)
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
		pos.y += 0.02f;//少し浮かせて描画

		//見やすいために少し小さくする
		Matrix4x4 world = Matrix4x4::CreateScale(0.9f) * Matrix4x4::CreateTranslation(pos);
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