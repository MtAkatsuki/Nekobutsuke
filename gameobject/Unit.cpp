#include "Unit.h"
#include "../manager/GameContext.h"
#include "../manager/MapManager.h"
#include "../manager/TurnManager.h"
#include "../enum class/TurnState.h"
#include "../ui/DamageNumberManager.h"

Unit::Unit(GameContext* context) :GameObject(context) {
	m_context->GetTurnManager()->RegisterObserver
	([this](TurnState state) {this->OnTurnChanged(state); });

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

	m_facing = DirOffset::FromVector(dir.x, dir.z);

	float angleOffset = PI;

	float baseAngle = static_cast<int>(m_facing) * (PI / 2.0f);
	//直ぐに向き変える
	m_srt.rot.y = baseAngle + angleOffset;
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

//子クラスでオーバーライドする
void Unit::StartTurn(){}

void Unit::EndTurn(){}

void Unit::OnTurnChanged(TurnState state){}