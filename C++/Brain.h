#pragma once
#include "stdafx.h"
#include "IExamPlugin.h"
#include "Exam_HelperStructs.h"
#include "SteeringBehaviors.h"

class IExamInterface;
class InventoryManager;
namespace Elite
{
	class Blackboard;
	class BehaviorTree;
}

struct HouseInfoExtended : HouseInfo
{
	int ItemsPickedUp;
	Elite::Vector2 Corners[4];
	bool CornersSeen[4];
};

struct EnemyInfoExtended : EnemyInfo
{
	bool InSight;
	bool InGrabRange;
	float OffSight;
};

enum class eTargetType
{
	Null,
	Exploration,
	House,
	Item,
	Enemy,
	Escape,
};

class Brain
{
public:
	Brain();
	~Brain();

	void Initialize(IExamInterface* pInterface);
	SteeringPlugin_Output Update(const float dt);
	void DrawDebug() const;

	Brain(const Brain& other) = delete;
	Brain(Brain&& other) = delete;
	Brain& operator=(const Brain& other) = delete;
	Brain& operator=(Brain&& other) = delete;

private:
	bool m_IsInitialized;
	IExamInterface* m_pInterface;
	InventoryManager* m_pInventory;
	std::vector<Elite::Vector2> m_KnownGarbage;
	std::vector<Elite::Vector2> m_KnownPistols;
	std::vector<Elite::Vector2> m_KnownMedKits;
	std::vector<Elite::Vector2> m_KnownFood;
	std::vector<Elite::Vector2> m_UnknownItems;
	std::vector<HouseInfoExtended> m_Houses;
	int m_CurrentHouseIdx;
	bool m_WasInHouse;
	bool m_RunMode;
	Elite::Vector2 m_Forward;
	Elite::Vector2 m_Target;
	Elite::Vector2 m_ExplorationTarget;
	std::vector<Elite::Vector2> m_ExploredTargets;
	Elite::Vector2 m_LatestPosition;
	Elite::Vector2 m_StuckTarget;
	float m_StuckProgress;
	float m_StuckCoolDown;
	const float m_StuckDistance = 2.f;
	bool m_IsStuck;
	bool m_HasFullStamina;
	float m_BittenTime = 0.f;
	std::vector<EnemyInfoExtended> m_Enemies;
	const int m_HouseCoolDownItemAmount = 35;

	SteeringBehavior* m_pMovement;
	SteeringBehavior* m_pOrientation;
	Seek* m_pSeek;
	Flee* m_pFlee;
	ForwardScanning* m_pForwardScanning;
	FullScanning* m_pFullScanning;
	RotateIntoFront* m_pRotateIntoFront;
	RotateIntoVision* m_pRotateIntoVision;

	Elite::Blackboard* m_pBlackboard;
	Elite::BehaviorTree* m_pBehaviorTree;


	void HandleStuck(const float dt);
	void HandleFovEntities();
	void HandleFovHouses();
	
	void HandleItem(const EntityInfo& entity);
	void HandleEnemy(const EntityInfo& entity);
	void HandlePurgeZone(const EntityInfo& entity);
	void UpdateEnemies(const float dt);

	void UpdateHouses();
	void UpdateCurrentHouse(const size_t idx);
	int GetNextHouse() const;
	Elite::Vector2 GetNearestCorner(const Elite::Vector2& pos, const HouseInfoExtended& house, size_t& idx, const bool ignoreSeen = false);

	void UpdateExplorationTarget();
	void UpdateItemTargets();
	void UpdateBlackboard();

	SteeringPlugin_Output CalculateSteering(const float dt) const;

	bool GetNearestUnknownItem(Elite::Vector2& target) const;

	vector<HouseInfo> GetHousesInFOV() const;
	vector<EntityInfo> GetEntitiesInFOV() const;

	void InitializeBehaviorTree();
	void InitializeBlackboard();
	void CleanBlackboard();
};

