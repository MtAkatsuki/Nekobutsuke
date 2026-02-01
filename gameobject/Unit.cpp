#include "Unit.h"
#include "../manager/GameContext.h"
#include "../manager/MapManager.h"
#include "../manager/TurnManager.h"
#include "../enum class/TurnState.h"
#include "../ui/DamageNumberManager.h"

Unit::Unit(GameContext* context) :GameObject(context) {
	if (m_context && m_context->GetTurnManager()) {
		m_context->GetTurnManager()->RegisterObserver(
			[this](TurnState state) { this->OnTurnChanged(state); }
		);
	}
	else {
		std::cerr << "[Error] Unit created but TurnManager is NULL! Observer failed." << std::endl;
	}
	m_hpBar = std::make_unique<HPBar>();
	m_hpBar->Init(context);
}

bool Unit::IsValidMoveTarget(int targetX, int targetZ) {
	if (m_context->GetMapManager() == nullptr) return false;

	bool isMapWalkable = m_context->GetMapManager()->IsWalkable(targetX, targetZ);

	return isMapWalkable;
}

void Unit::TakeDamage(int damage, Unit* attacker)
{
	if (damage < 0) return;
	m_currentHP = std::max(0, m_currentHP - damage);
	if (m_context->GetDamageManager()) {
		Vector3 headPos = m_srt.pos;
		headPos.y += 1.0f;

		headPos.x += ((rand() % 10) / 10.0f - 0.5f) * 0.5f;

		m_context->GetDamageManager()->SpawnDamage(headPos, damage);
	}
	OnHpChanged();
}

void Unit::SetFacingFromVector(const Vector3& dir) {
	//ベクトルの長さが小さい過ぎると、向きは変更しないになる、フレッシュを防ぐ
	if (dir.LengthSquared() < 0.0001f)return;

	Direction newDir = DirOffset::FromVector(dir.x, dir.z);

	SetFacing(newDir);
}


void Unit::SetModelRenderers(CStaticMeshRenderer* front, CStaticMeshRenderer* back)
{
	m_frontRenderer = front;
	m_backRenderer = back;
	// --- マテリアルカラーを強制的に白（明るく）に設定 ---
	auto ForceWhiteMaterial = [](CStaticMeshRenderer* renderer) {
		if (!renderer) return;

		// モデルのマテリアル設定を取得（通常はインデックス0のみを想定）
		// ※複数のマテリアルがある場合はループ処理が必要だが、今回は0番目のみ対象とする
		if (auto* mat = renderer->GetMaterial(0)) {
			MATERIAL m = mat->GetData();

			// 【重要】光の計算による黒ずみを防ぐため、基本色を「白」にする
			m.Diffuse = Color(1.0f, 1.0f, 1.0f, 1.0f);  // ディフューズ（拡散反射光）を真っ白に
			m.Ambient = Color(1.0f, 1.0f, 1.0f, 1.0f);  // アンビエント（環境光）も真っ白に
			m.Emission = Color(0.1f, 0.1f, 0.1f, 1.0f); // エミッシブ（自己発光）はオフ
			m.TextureEnable = TRUE;                     // テクスチャ表示を確実に有効化

			mat->SetMaterial(m);
		}
		};

	// 前面・背面の両方のレンダラーに適用
	ForceWhiteMaterial(m_frontRenderer);
	ForceWhiteMaterial(m_backRenderer);
	// -------------------------
	// デフォルトで正面モデルを使用
	m_currRenderer = m_frontRenderer;
	m_srt.scale = Vector3(1.0f, 1.0f, 1.0f);
	m_srt.rot = Vector3(0.0f, 0.0f, 0.0f);
}

void Unit::StartAttackAnimation(const Vector3& targetPos) {
	m_animStartPos = m_srt.pos;
	m_animTimer = 0.0f;
	m_animHasHit = false;

	//攻撃用に、目標方向ぶつかる
	Vector3 dir = targetPos - m_animStartPos;
	//正規化の前に、0じゃないを検査する
	if (dir.Length() > 0.001f)dir.Normalize();

	m_animLungePos = m_animStartPos + dir * 0.7f;

	SetFacingFromVector(dir);
}

bool Unit::UpdateAttackAnimation(float dt, std::function<void()> onImpact) 
{
	
	float deltaSeconds = dt / 1000.0f;//秒の変換
	m_animTimer += deltaSeconds;
	//ダメージのコールバック
	if (!m_animHasHit && m_animTimer >= TIME_IMPACT) 
	{
		m_animHasHit = true;
		if (onImpact)onImpact();
	}

	bool isFinished = false;

	if (m_animTimer < TIME_LUNGE)
	{
		float t = m_animTimer / TIME_LUNGE;
		m_srt.pos = Vector3::Lerp(m_animStartPos, m_animLungePos, t);
	}
	else if (m_animTimer < TIME_RETURN) 
	{
		m_srt.pos = m_animLungePos;//ぶつかってるように停止する
	}
	else if (m_animTimer < TIME_END) 
	{
		float t = (m_animTimer - TIME_RETURN) / (TIME_END - TIME_RETURN);
		m_srt.pos = Vector3::Lerp(m_animLungePos, m_animStartPos, t);
	}
	else 
	{
		m_srt.pos = m_animStartPos;
		isFinished = true;
	}

	UpdateWorldMatrix();
	Renderer::SetWorldMatrix(&m_WorldMatrix);

	return isFinished;
}

void Unit::StartBumpAnimation(const Vector3& targetPos)
{
	//アタック アニメションと同じ、ただ向きはレセットしない
	m_animStartPos = m_srt.pos;
	m_animTimer = 0.0f;
	m_slideTimer = 0.0f;
	m_animHasHit = false;

	Vector3 dir = targetPos - m_animStartPos;
	if (dir.Length() > 0.001f) dir.Normalize();

	m_animLungePos = m_animStartPos + dir * 0.7f;

}

void Unit::StartSlideAnimation(const Vector3& targetPos)
{	//攻撃受け位置変化アニメションの初期化
	m_slideStartPos = m_srt.pos;
	m_slideEndPos = targetPos;
	m_animTimer = 0.0f;
	m_slideTimer = 0.0f;
}

bool Unit::UpdateSlideAnimation(uint64_t dt) 
{
	float deltaSeconds = dt / 1000.0f;
	m_slideTimer += deltaSeconds;

	const float TIME_SLIDE = 0.2f;
	//アニメション実行中
	if(m_slideTimer<TIME_SLIDE)
	{
		float t = m_slideTimer / TIME_SLIDE;
		t = 1.0f - std::pow(1.0f - t, 2.0f);
		m_srt.pos = Vector3::Lerp(m_slideStartPos, m_slideEndPos, t);

		UpdateWorldMatrix();
		Renderer::SetWorldMatrix(&m_WorldMatrix);
		return false;
	}
	else //アニメション完了
	{
		m_srt.pos = m_slideEndPos;
		UpdateWorldMatrix();
		Renderer::SetWorldMatrix(&m_WorldMatrix);
		return true;
	}
}

void Unit::DrawUI()
{	//描画前の検査
	if (!m_hpBar) return;
	if (m_currentHP <= 0) return;

	// ブロックタイプ選択
	HPBar::BarType type = HPBar::BarType::Green;
	if (m_team == Team::Enemy) {
		type = HPBar::BarType::Red;
	}

	m_hpBar->Draw(m_srt.pos, m_currentHP, m_maxHP, type);
}

// gameobject/Unit.cpp

void Unit::UpdateFlipAnimation(float dt) {
	// アニメーション中でない場合は Y軸回転をリセット
	if (!m_isFlipping) {
		m_srt.rot.y = 0.0f;
		return;
	}

	m_flipTimer += dt;
	float t = m_flipTimer / FLIP_DURATION; // 正規化された時間 (0.0 -> 1.0)
	float visualRotY = 0.0f;

	// --- フェーズ 1: 0度 -> 90度 (薄くなる) ---
	if (t < 0.5f) {
		float phaseT = t / 0.5f;
		visualRotY = std::lerp(0.0f, PI / 2.0f, phaseT);
	}
	// --- フェーズ 2: 90度 -> 0度 (厚みが戻る、メッシュ切り替え) ---
	else {
		// --- 臨界点：メッシュの切り替えとスケールの計算 ---
		if (!m_hasSwappedMesh) {
			m_facing = m_nextFacing;
			m_hasSwappedMesh = true;

			// 「左右の向きの記憶」を更新 (北向き以外の場合のみ)
			if (m_facing == Direction::East || m_facing == Direction::South) {
				m_isFacingRight = true;  // 右向きとして記録
			}
			else if (m_facing == Direction::West) {
				m_isFacingRight = false; // 左向きとして記録
			}
			// ※ North（北）の場合は、直前の m_isFacingRight を保持する

			//  向き（Facing）に応じてレンダラーを選択
			//  記憶された向き（m_isFacingRight）に基づいて ScaleX を決定

			float targetScaleX = 1.0f;

			if (m_facing == Direction::North) {
				// --- 背面モード ---
				m_currRenderer = m_backRenderer;

				// 背面モデルの標準が「左向き」であると仮定したルール
				if (m_isFacingRight) {
					// 「右向きの背面」を表示したい場合 -> 反転（ミラーリング）
					targetScaleX = -1.0f;
				}
				else {
					// 「左向きの背面」を表示したい場合 -> 標準
					targetScaleX = 1.0f;
				}
			}
			else {
				// --- 正面モード ---
				m_currRenderer = m_frontRenderer;

				// 正面モデルの標準が「右向き（南/東）」であると仮定
				if (m_isFacingRight) {
					targetScaleX = 1.0f;  // 標準
				}
				else {
					targetScaleX = -1.0f; // 反転（ミラーリング）
				}
			}

			// スケールを適用
			m_srt.scale.x = targetScaleX;
		}

		// 後半の回転計算
		float phaseT = (t - 0.5f) / 0.5f;
		visualRotY = std::lerp(PI / 2.0f, 0.0f, phaseT);
	}

	// アニメーション終了処理
	if (t >= 1.0f) {
		m_isFlipping = false;
		visualRotY = 0.0f;
	}

	// 最終的なY軸回転を適用
	m_srt.rot.y = visualRotY;
}

void Unit::DrawModel() {
	// レンダラーが初期化されていない場合は描画をスキップ
	if (!m_currRenderer) return;

	// ワールド行列を設定（座標、回転、スケーリングを反映）
	Renderer::SetWorldMatrix(&m_WorldMatrix);

	// 現在アクティブなレンダラーを使用して描画を実行
	m_currRenderer->Draw();
}

void Unit::SetFacing(Direction newDir)
{
	// 向きが変わらない、または既にアニメーション中なら何もしない
	if (m_facing == newDir || m_isFlipping) return;

	// --- アニメーション省略判定 (南 <-> 東) ---
	// 南(正面) と 東(正面) は見た目が同じ(ScaleX=1)なのでアニメーション不要
	bool isCurrentNormal = (m_facing == Direction::South || m_facing == Direction::East);
	bool isNextNormal = (newDir == Direction::South || newDir == Direction::East);

	if (isCurrentNormal && isNextNormal) {
		m_facing = newDir;
		// 即座に向きを更新して終了
		return;
	}

	// --- アニメーションタイプの決定 ---
	// 北（背面）が絡む場合は「単純な反転 (Simple)」
	if (m_facing == Direction::North || newDir == Direction::North) {
		m_currentFlipStyle = FlipStyle::Simple;
	}
	// 西（反転）が絡む場合（主に西<->東、西<->南）は「スイング反転 (Swing)」
	else {
		m_currentFlipStyle = FlipStyle::Swing;
	}

	// アニメーション開始
	m_nextFacing = newDir;
	m_isFlipping = true;
	m_flipTimer = 0.0f;
	m_hasSwappedMesh = false;
}

//子クラスでオーバーライドする
void Unit::StartTurn(){}

void Unit::EndTurn(){}

void Unit::OnTurnChanged(TurnState state){}