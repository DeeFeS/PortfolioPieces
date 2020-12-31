#include "stdafx.h"
#include "Brain.h"
#include <IExamInterface.h>
#include "InventoryManager.h"
#include <algorithm>
#include "HelperFunctions.h"
#include "BlackboardDefinitions.h"
#include "BehaviorDefinitions.h"
#include "EBlackboard.h"
#include "EBehaviorTree.h"

using namespace Elite;

Brain::Brain()
	: m_pInterface{ nullptr }
	, m_pInventory{ nullptr }
	, m_pBlackboard{ nullptr }
	, m_pBehaviorTree{ nullptr }
	, m_pSeek{ nullptr }
	, m_pFlee{ nullptr }
	, m_pForwardScanning{ nullptr }
	, m_pFullScanning{ nullptr }
	, m_pRotateIntoFront{ nullptr }
	, m_pRotateIntoVision{ nullptr }
	, m_pMovement{ nullptr }
	, m_pOrientation{ nullptr }
	, m_KnownPistols{ }
	, m_KnownGarbage{ }
	, m_KnownMedKits{ }
	, m_KnownFood{ }
	, m_CurrentHouseIdx{ -1 }
	, m_StuckCoolDown{ 0.f }
	, m_StuckProgress{ 0.f }
	, m_IsInitialized{ false }
	, m_WasInHouse{ false }
	, m_RunMode{ false }
	, m_IsStuck{ false }
	, m_HasFullStamina{ true }
	, m_Target{ }
{}

Brain::~Brain()
{
	m_pInterface = nullptr;
	SAFE_DELETE(m_pInventory);
	SAFE_DELETE(m_pSeek);
	SAFE_DELETE(m_pBehaviorTree);
	CleanBlackboard();
}

void Brain::Initialize(IExamInterface* pInterface)
{
	if (!m_IsInitialized)
	{
		if (!pInterface)
			return;
		m_pInterface = pInterface;
		m_pInventory = new InventoryManager{ m_pInterface };

		m_pSeek = new Seek();
		m_pSeek->SetInterface(m_pInterface);
		m_pSeek->SetTarget(m_pInterface->Agent_GetInfo().Position);

		m_pMovement = m_pSeek;

		m_pFlee = new Flee();
		m_pFlee->SetInterface(m_pInterface);
		m_pFlee->SetTarget(m_pInterface->Agent_GetInfo().Position);

		m_pForwardScanning = new ForwardScanning();
		m_pForwardScanning->SetInterface(m_pInterface);
		m_pForwardScanning->SetAngle(80.f);

		m_pOrientation = m_pForwardScanning;

		m_pFullScanning = new FullScanning();
		m_pFullScanning->SetInterface(m_pInterface);

		m_pRotateIntoFront = new RotateIntoFront();
		m_pRotateIntoFront->SetInterface(m_pInterface);
		m_pRotateIntoFront->SetTarget(m_pInterface->Agent_GetInfo().Position);

		m_pRotateIntoVision = new RotateIntoVision();
		m_pRotateIntoVision->SetInterface(m_pInterface);
		m_pRotateIntoVision->SetTarget(m_pInterface->Agent_GetInfo().Position);

		m_LatestPosition = m_pInterface->Agent_GetInfo().Position;

		InitializeBehaviorTree();

		m_IsInitialized = true;
	}
}


void Brain::DrawDebug() const
{
	const auto& agent = m_pInterface->Agent_GetInfo();

	// Forward
	m_pInterface->Draw_Direction(agent.Position, m_Forward, agent.FOV_Range, { 0, 1, 0 });
	//*/

	// Target
	m_pInterface->Draw_Circle(m_LatestPosition, m_StuckDistance, { 0.5f, 0, 0 });
	m_pInterface->Draw_SolidCircle(m_pMovement->GetTarget(), 2.f, { 0, 0 }, { 1, 1, 1 });
	m_pInterface->Draw_SolidCircle(m_pInterface->NavMesh_GetClosestPathPoint(m_pSeek->GetTarget()), 0.25f, { 0, 0 }, { 1, 1, 1 });
	if (m_pSeek->GetTarget() == ZeroVector2)
		m_pInterface->Draw_SolidCircle(m_pSeek->GetTarget(), 2.f, { 0, 0 }, { 1, 0, 0 });
	if (m_pOrientation == (SteeringBehavior*)m_pRotateIntoFront)
		m_pInterface->Draw_SolidCircle(m_pRotateIntoFront->GetTarget(), 1.25f, { 0, 0 }, { 0.5, 0, 0 });
	if (m_pOrientation == (SteeringBehavior*)m_pRotateIntoVision)
		m_pInterface->Draw_SolidCircle(m_pRotateIntoVision->GetTarget(), 1.25f, { 0, 0 }, { 0, 0, 0.5 });
	//*/

	// World
	const auto& worldInfo = m_pInterface->World_GetInfo();
	auto half = worldInfo.Dimensions / 2.f;
	const std::vector<Vector2> world =
	{
		worldInfo.Center + Vector2{ -half.x, -half.y },
		worldInfo.Center + Vector2{ -half.x, half.y },
		worldInfo.Center + Vector2{ half.x, half.y },
		worldInfo.Center + Vector2{ half.x, -half.y },
	};
	m_pInterface->Draw_Polygon(world.data(), 4, { 0, 0, 0 });
	//*/

	// Houses
	for (size_t i = 0; i < m_Houses.size(); i++)
	{
		m_pInterface->Draw_Polygon(m_Houses[i].Corners, 4, { 0, 0, 0 });
		for (size_t j = 0; j < 4; j++)
		{
			m_pInterface->Draw_SolidCircle(m_Houses[i].Corners[j], 0.25f, { 0, 0 }, m_Houses[i].CornersSeen[j] ? Vector3{ 0, 1, 0 } : Vector3{ 1, 0, 0 });
		}
	}
	//*/

	// Items
	const float itemSize = 1.5f;
	for (size_t i = 0; i < m_KnownMedKits.size(); i++)
		m_pInterface->Draw_SolidCircle(m_KnownMedKits[i], itemSize, { 0, 0 }, { 1, 0, 0 });
	for (size_t i = 0; i < m_KnownFood.size(); i++)
		m_pInterface->Draw_SolidCircle(m_KnownFood[i], itemSize, { 0, 0 }, { 0, 1, 0 });
	for (size_t i = 0; i < m_KnownPistols.size(); i++)
		m_pInterface->Draw_SolidCircle(m_KnownPistols[i], itemSize, { 0, 0 }, { 0, 0, 1 });
	for (size_t i = 0; i < m_KnownGarbage.size(); i++)
		m_pInterface->Draw_SolidCircle(m_KnownGarbage[i], itemSize, { 0, 0 }, { 1, 0.5, 0 });
	for (size_t i = 0; i < m_UnknownItems.size(); i++)
		m_pInterface->Draw_SolidCircle(m_UnknownItems[i], itemSize, { 0, 0 }, { 1, 0, 1 });
	//*/
}

SteeringPlugin_Output Brain::Update(const float dt)
{
	m_RunMode = false;
	m_IsStuck = false;
	m_Forward = RotateVector({ 0.f,-1.f }, m_pInterface->Agent_GetInfo().Orientation);
	m_pInventory->Update();
	HandleStuck(dt);
	HandleFovEntities();
	HandleFovHouses();
	UpdateExplorationTarget();
	UpdateEnemies(dt);
	UpdateHouses();
	UpdateItemTargets();
	UpdateBlackboard();
	m_pBehaviorTree->Update();

	return CalculateSteering(dt);
}

SteeringPlugin_Output Brain::CalculateSteering(const float dt) const
{
	SteeringPlugin_Output ret{};
	SteeringPlugin_Output buffer{};

	ret = m_pMovement->CalculateSteering(dt);
	buffer = m_pOrientation->CalculateSteering(dt);
	ret.AutoOrientate = buffer.AutoOrientate;
	ret.AngularVelocity = buffer.AngularVelocity;
	ret.RunMode = m_RunMode;
	return ret;
}

#pragma region HOUSES --------------------------------------------------------------------------------
void Brain::HandleFovHouses()
{
	auto agentPos = m_pInterface->Agent_GetInfo().Position;
	auto houses = GetHousesInFOV();
	for (size_t j = 0; j < houses.size(); j++)
	{
		int idx = -1;
		for (int i = 0; i < (int)m_Houses.size(); i++)
		{
			if (houses[j].Center == m_Houses[i].Center)
			{
				idx = i;
				break;
			}
		}

		if (idx == -1)
		{
			HouseInfoExtended newHouse{};
			newHouse.Center = houses[j].Center;
			newHouse.Size = houses[j].Size;
			newHouse.ItemsPickedUp = -666;

			const float offset = 3.f;
			Vector2 half = newHouse.Size / 2.f - Vector2{ offset, offset };
			newHouse.Corners[0] = newHouse.Center + Vector2{ -half.x, -half.y };
			newHouse.Corners[1] = newHouse.Center + Vector2{ -half.x, half.y };
			newHouse.Corners[2] = newHouse.Center + Vector2{ half.x, half.y };
			newHouse.Corners[3] = newHouse.Center + Vector2{ half.x, -half.y };

			m_Houses.push_back(newHouse);
		}
	}
}

void Brain::UpdateHouses()
{
	auto agentPos = m_pInterface->Agent_GetInfo().Position;
	bool isInHouse = m_pInterface->Agent_GetInfo().IsInHouse;

	bool hasHouse = false;

	for (size_t i = 0; i < m_Houses.size(); i++)
	{
		if (isInHouse && InBounds(agentPos, m_Houses[i].Center, m_Houses[i].Size))
		{
			if (i != m_CurrentHouseIdx)
			{
				m_CurrentHouseIdx = i;
				if (m_pInterface->World_GetStats().NumItemsPickUp - m_Houses[m_CurrentHouseIdx].ItemsPickedUp > m_HouseCoolDownItemAmount)
				{
					m_Houses[i].CornersSeen[0] = false;
					m_Houses[i].CornersSeen[1] = false;
					m_Houses[i].CornersSeen[2] = false;
					m_Houses[i].CornersSeen[3] = false;
				}
				m_Houses[m_CurrentHouseIdx].ItemsPickedUp = m_pInterface->World_GetStats().NumItemsPickUp;
			}
			UpdateCurrentHouse(i);
		}
	}

	if (!isInHouse && m_WasInHouse)
	{
		m_Houses[m_CurrentHouseIdx].ItemsPickedUp = m_pInterface->World_GetStats().NumItemsPickUp;
		m_CurrentHouseIdx = -1;
	}

	m_WasInHouse = isInHouse;

	int idx = GetNextHouse();
	m_pBlackboard->ChangeData(bb_KnowsHouse, idx != -1);
	if (idx != -1)
	{
		size_t buffer;
		Vector2 corner = GetNearestCorner(agentPos, m_Houses[idx], buffer, false);
		m_pBlackboard->ChangeData(bb_HouseCloserThanExploration, agentPos.DistanceSquared(m_Houses[idx].Center) < agentPos.DistanceSquared(m_ExplorationTarget));
		m_pBlackboard->ChangeData(bb_HouseLocation, corner);
		m_pBlackboard->ChangeData(bb_NextHouseIsExplored, m_Houses[idx].CornersSeen[0] && m_Houses[idx].CornersSeen[1] && m_Houses[idx].CornersSeen[2] && m_Houses[idx].CornersSeen[3]);
	}
}

void Brain::UpdateCurrentHouse(const size_t idx)
{
	const auto& agent = m_pInterface->Agent_GetInfo();
	float right = agent.Orientation - agent.FOV_Angle / 2.f;
	float left = agent.Orientation + agent.FOV_Angle / 2.f;


	m_pBlackboard->ChangeData(bb_IsInHouse, agent.IsInHouse);

	HouseInfoExtended& house = m_Houses[idx];

	bool allExplored = true;
	Vector2 corner{};
	float minSq = FLT_MAX;


	const float piHalved = F_PI / 2.f;
	for (size_t j = 0; j < 4; j++)
	{
		if (house.CornersSeen[j])
			continue;

		float disSq = agent.Position.DistanceSquared(house.Corners[j]);
		if (disSq < powf(m_pInterface->Agent_GetInfo().FOV_Range, 2.f))
		{

			auto cornerDir = (house.Corners[j] - agent.Position).GetNormalized();
			float angle = atan2f(cornerDir.y, cornerDir.x) + piHalved;

			if (AngleIsInbetweenInDegree(ToDegrees(angle), ToDegrees(left), ToDegrees(right)))
				house.CornersSeen[j] = true;
		}
		if (disSq < powf(m_pInterface->Agent_GetInfo().GrabRange, 2.f))
			house.CornersSeen[j] = true;

		if (!house.CornersSeen[j])
		{
			allExplored = false;
			if (disSq < minSq)
			{
				minSq = disSq;
				corner = house.Corners[j];
			}
		}
	}

	m_pBlackboard->ChangeData(bb_HouseIsExplored, allExplored);
	m_pBlackboard->ChangeData(bb_NearestUnexploredCorner, corner);
}

void Brain::UpdateExplorationTarget()
{
	const auto& world = m_pInterface->World_GetInfo();
	const auto half = world.Dimensions / 2.f;

	const auto& agent = m_pInterface->Agent_GetInfo();

	if (m_ExplorationTarget == m_StuckTarget)
	{
		m_ExplorationTarget = { randomFloat(world.Dimensions.x), randomFloat(world.Dimensions.y) };
		m_ExplorationTarget += world.Center - half;
		m_pBlackboard->ChangeData(bb_ExplorationTarget, m_ExplorationTarget);
	}
	else if (agent.Position.DistanceSquared(m_ExplorationTarget) < powf(m_pInterface->Agent_GetInfo().FOV_Range, 2.f))
	{
		m_ExploredTargets.push_back(m_ExplorationTarget);
		m_ExplorationTarget = { randomFloat(world.Dimensions.x), randomFloat(world.Dimensions.y) };
		m_ExplorationTarget += world.Center - half;
		m_pBlackboard->ChangeData(bb_ExplorationTarget, m_ExplorationTarget);
	}
}

void Brain::UpdateItemTargets()
{
	const auto agentPos = m_pInterface->Agent_GetInfo().Position;

	float nearestSq = FLT_MAX;
	int nearestIdx = -1;
	eItemType nearestType = eItemType::RANDOM_DROP;

	auto GetNearest = [&nearestSq, &nearestIdx, &nearestType](const std::vector<Vector2>& v, const Vector2& pos, const eItemType type) -> Vector2
	{
		int idx = -1;
		float minSq = FLT_MAX;
		for (int i = 0; i < int(v.size()); i++)
		{
			float disSq = pos.DistanceSquared(v[i]);
			if (disSq < minSq)
			{
				minSq = disSq;
				idx = i;
			}
		}
		if (minSq < nearestSq)
		{
			nearestSq = minSq;
			nearestIdx = idx;
			nearestType = type;
		}
		return idx != -1 ? v[idx] : ZeroVector2;
	};

	m_pBlackboard->ChangeData(bb_KnowsFood, !m_KnownFood.empty());
	m_pBlackboard->ChangeData(bb_KnowsPistol, !m_KnownPistols.empty());
	m_pBlackboard->ChangeData(bb_KnowsMedKit, !m_KnownMedKits.empty());
	m_pBlackboard->ChangeData(bb_KnowsGarbage, !m_KnownGarbage.empty());

	m_pBlackboard->ChangeData(bb_NearestFood, GetNearest(m_KnownFood, agentPos, eItemType::FOOD));
	m_pBlackboard->ChangeData(bb_NearestPistol, GetNearest(m_KnownPistols, agentPos, eItemType::PISTOL));
	m_pBlackboard->ChangeData(bb_NearestMedKit, GetNearest(m_KnownMedKits, agentPos, eItemType::MEDKIT));
	m_pBlackboard->ChangeData(bb_NearestGarbage, GetNearest(m_KnownGarbage, agentPos, eItemType::GARBAGE));

	m_pBlackboard->ChangeData(bb_NearestItem, nearestType);

	Vector2 item;
	if (GetNearestUnknownItem(item))
	{
		m_pBlackboard->ChangeData(bb_HasUnknownItem, true);
		m_pBlackboard->ChangeData(bb_NearestUnknownItem, item);
		if (agentPos.DistanceSquared(item) < powf(m_pInterface->Agent_GetInfo().FOV_Range, 2.f))
			m_pBlackboard->ChangeData(bb_UnknownItemInArea, true);
	}
	else
	{
		m_pBlackboard->ChangeData(bb_HasUnknownItem, false);
		m_pBlackboard->ChangeData(bb_UnknownItemInArea, false);
	}
}

int Brain::GetNextHouse() const
{
	const auto agentPos = m_pInterface->Agent_GetInfo().Position;
	const auto currentPickedUp = int(m_pInterface->World_GetStats().NumItemsPickUp);
	float minDistanceSq = FLT_MAX;
	int maxItemPassed = -1;
	int idx = -1;
	int idxNearest = -1;
	for (size_t i = 0; i < m_Houses.size(); i++)
	{
		if (i == m_CurrentHouseIdx)
			continue;
		float distanceSq = agentPos.DistanceSquared(m_Houses[i].Center);
		if (distanceSq < minDistanceSq)
		{
			minDistanceSq = distanceSq;
			idxNearest = i;
		}
		int itemsPassed = int(currentPickedUp - m_Houses[i].ItemsPickedUp);
		if (itemsPassed == maxItemPassed)
		{
			// Check if closest unvisited house
			float otherSq = agentPos.DistanceSquared(m_Houses[idx].Center);
			maxItemPassed = itemsPassed;
			if (distanceSq < otherSq)
				idx = i;
		}
		else if (itemsPassed > maxItemPassed)
		{
			maxItemPassed = itemsPassed;
			idx = i;
		}
	}

	if (maxItemPassed < m_HouseCoolDownItemAmount)
		return -1;

	if (maxItemPassed < currentPickedUp)
	{
		if (currentPickedUp - m_Houses[idxNearest].ItemsPickedUp > m_HouseCoolDownItemAmount)
			idx = idxNearest;
	}

	return idx;
}

Vector2 Brain::GetNearestCorner(const Vector2& pos, const HouseInfoExtended& house, size_t& idx, const bool ignoreSeen)
{
	float minSq = FLT_MAX;
	for (size_t i = 0; i < 4; i++)
	{
		if (house.Corners[i] == m_StuckTarget)
			continue;
		if (ignoreSeen && house.CornersSeen[i])
			continue;
		float disSq = house.Corners[i].DistanceSquared(pos);
		if (disSq < minSq)
		{
			minSq = disSq;
			idx = i;
		}
	}

	return house.Corners[idx];
}
vector<HouseInfo> Brain::GetHousesInFOV() const
{
	vector<HouseInfo> vHousesInFOV = {};

	HouseInfo hi = {};
	for (int i = 0;; ++i)
	{
		if (m_pInterface->Fov_GetHouseByIndex(i, hi))
		{
			vHousesInFOV.push_back(hi);
			continue;
		}

		break;
	}

	return vHousesInFOV;
}
#pragma endregion

#pragma region ENITIES -------------------------------------------------------------------------------
void Brain::HandleStuck(const float dt)
{
	if (m_StuckCoolDown > 0.f)
	{
		m_StuckCoolDown -= dt;
		if (m_StuckCoolDown <= 0.f)
		{
			m_StuckTarget = { FLT_MAX, FLT_MAX };
			m_pBlackboard->ChangeData(bb_StuckTarget, m_StuckTarget);
		}
	}

	if (m_LatestPosition.DistanceSquared(m_pInterface->Agent_GetInfo().Position) > powf(m_StuckDistance, 2.f))
	{
		m_pBlackboard->ChangeData(bb_IsStuck, false);
		m_LatestPosition = m_pInterface->Agent_GetInfo().Position;
		m_StuckProgress = 0.f;
		return;
	}

	m_StuckProgress += dt;
	const float timeTillStuck = 3.f;
	if (m_StuckProgress > timeTillStuck)
	{
		const float stuckCoolDown = 30.f;
		m_StuckCoolDown = stuckCoolDown;
		m_IsStuck = true;
		m_StuckTarget = m_pMovement->GetTarget();
		m_pBlackboard->ChangeData(bb_StuckTarget, m_StuckTarget);
		m_pBlackboard->ChangeData(bb_IsStuck, true);
		m_StuckProgress = 0.f;
	}
}

void Brain::HandleFovEntities()
{
	m_pBlackboard->ChangeData(bb_IsInPurgeZone, false);
	m_pBlackboard->ChangeData(bb_KnewEnemy, !m_Enemies.empty());
	m_Enemies.clear();

	auto entities = GetEntitiesInFOV();
	for (size_t i = 0; i < entities.size(); i++)
	{
		switch (entities[i].Type)
		{
		case eEntityType::ITEM:
			HandleItem(entities[i]);
			break;
		case eEntityType::ENEMY:
			HandleEnemy(entities[i]);
			break;
		case eEntityType::PURGEZONE:
			HandlePurgeZone(entities[i]);
			break;
		default:
			break;
		}
	}
}

void Brain::HandleItem(const EntityInfo& entity)
{
	auto agent = m_pInterface->Agent_GetInfo();
	const float disSq = entity.Location.DistanceSquared(agent.Position);
	if (disSq < powf(agent.GrabRange, 2.f))
	{
		ItemInfo item;
		if (m_pInterface->Item_Grab(entity, item))
		{
			m_StuckProgress = 0.f;

			int idx = -1;
			std::vector<Vector2>* pVec = &m_KnownPistols;
			switch (item.Type)
			{
			case eItemType::PISTOL: pVec = &m_KnownPistols; break;
			case eItemType::MEDKIT: pVec = &m_KnownMedKits; break;
			case eItemType::FOOD: pVec = &m_KnownFood; break;
			case eItemType::GARBAGE: pVec = &m_KnownGarbage; break;
			default:
				std::cout << "UNKNOWN ITEM TYPE!" << std::endl;
				return;
			}

			for (int i = 0; i < (int)pVec->size(); i++)
			{
				if (item.Location == pVec->at(i))
				{
					idx = i;
					break;
				}
			}

			if (idx == -1)
			{
				auto it = std::find(m_UnknownItems.cbegin(), m_UnknownItems.cend(), item.Location);
				if (it != m_UnknownItems.cend())
					m_UnknownItems.erase(it);
			}

			if (!m_pInventory->AddItem(item))
			{
				if (idx == -1)
				{
					pVec->push_back(item.Location);
				}
			}
			else
			{
				if (idx != -1)
				{
					pVec->at(idx) = pVec->back();
					pVec->pop_back();
				}
			}
		}
	}
	else
	{
		if (std::find(m_KnownFood.cbegin(), m_KnownFood.cend(), entity.Location) != m_KnownFood.cend()
			|| std::find(m_KnownGarbage.cbegin(), m_KnownGarbage.cend(), entity.Location) != m_KnownGarbage.cend()
			|| std::find(m_KnownMedKits.cbegin(), m_KnownMedKits.cend(), entity.Location) != m_KnownMedKits.cend()
			|| std::find(m_KnownPistols.cbegin(), m_KnownPistols.cend(), entity.Location) != m_KnownPistols.cend())
			return;

		auto it = std::find(m_UnknownItems.cbegin(), m_UnknownItems.cend(), entity.Location);
		if (it == m_UnknownItems.cend())
			m_UnknownItems.push_back(entity.Location);

		if (disSq < agent.Position.DistanceSquared(m_Target))
			m_Target = entity.Location;
	}
}

void Brain::HandleEnemy(const EntityInfo& entity)
{
	auto agent = m_pInterface->Agent_GetInfo();
	EnemyInfoExtended info;
	if (!m_pInterface->Enemy_GetInfo(entity, info))
		return;

	info.InSight = false;
	info.InGrabRange = false;
	info.OffSight = 0.f;

	const float piHalved = float(M_PI) / 2.f;

	auto enemyDir = (entity.Location - agent.Position).GetNormalized();

	enemyDir = Vector2{ enemyDir.y, -enemyDir.x }; // perp vector
	enemyDir *= info.Size / 2.f;

	auto toEnemyDir = entity.Location + enemyDir - agent.Position;
	float right = ToDegrees(atan2f(toEnemyDir.y, toEnemyDir.x) + piHalved);

	toEnemyDir = entity.Location - enemyDir - agent.Position;
	float left = ToDegrees(atan2f(toEnemyDir.y, toEnemyDir.x) + piHalved);

	float rotation = ToDegrees(agent.Orientation);
	if (AngleIsInbetweenInDegree(rotation, left, right))
		info.InSight = true;
	else
		info.OffSight = AngleDifferenceInDegrees(rotation, (left + right) / 2.f);

	info.InGrabRange = info.Location.DistanceSquared(agent.Position) < powf(agent.GrabRange, 2.f);

	if (!m_pInventory->HasUsedItemThisFrame() && info.InSight)
		m_pInventory->Shoot();

	m_Enemies.push_back(info);
}

void Brain::HandlePurgeZone(const EntityInfo& entity)
{
	PurgeZoneInfo pz;
	if (!m_pInterface->PurgeZone_GetInfo(entity, pz))
		return;

	float distance = m_pInterface->Agent_GetInfo().Position.Distance(pz.Center);
	if (distance < pz.Radius + m_pInterface->Agent_GetInfo().AgentSize + 1.f)
		m_pBlackboard->ChangeData(bb_IsInPurgeZone, true);

	m_pBlackboard->ChangeData(bb_PurgeZoneInfo, pz);
	m_pBlackboard->ChangeData(bb_PurgeZoneDistance, distance);
}

void Brain::UpdateEnemies(const float dt)
{
	const auto& agent = m_pInterface->Agent_GetInfo();
	const float prolongedBittenTime = 4.f;
	if (agent.Bitten)
		m_BittenTime = 4.f;

	m_BittenTime -= dt;

	if (m_Enemies.empty())
	{
		m_pBlackboard->ChangeData(bb_EnemyInSight, false);
		m_pBlackboard->ChangeData(bb_EnemyInRange, false);
		m_pBlackboard->ChangeData(bb_IsInCombat, m_BittenTime > 0.f);
		m_pBlackboard->ChangeData(bb_NearestEnemy, agent.Position - RotateVector(m_Forward, F_PI / 2.f));
		m_pBlackboard->ChangeData(bb_EnemyCenter,
			agent.Position - RotateVector(m_Forward, -(m_BittenTime / prolongedBittenTime) * F_PI * 2.f));
		return;
	}

	Vector2 nearestEnemy;
	size_t nearestIdx = 0;
	Vector2 center{};
	int grabRangeIdx = -1;
	bool hasOneInSight = false;
	float minSq = FLT_MAX;

	bool hasPistol = m_pInventory->HasItemOfType(eItemType::PISTOL);
	for (size_t i = 0; i < m_Enemies.size(); i++)
	{
		center += m_Enemies[i].Location;

		if (hasPistol)
		{
			if (!hasOneInSight)
			{
				if (m_Enemies[i].InSight)
				{
					nearestEnemy = m_Enemies[i].Location;
					hasOneInSight = true;
				}
				else if (m_Enemies[i].InGrabRange)
				{
					if (grabRangeIdx == -1)
						grabRangeIdx = i;
					else
					{
						if (abs(m_Enemies[i].OffSight) < abs(m_Enemies[grabRangeIdx].OffSight))
							grabRangeIdx = i;
					}
				}
				else if (grabRangeIdx == -1)
				{
					float disSq = m_Enemies[i].Location.DistanceSquared(agent.Position);
					if (disSq < minSq)
					{
						minSq = disSq;
						nearestIdx = i;
					}
				}
			}
		}
		else
		{
			float disSq = m_Enemies[i].Location.DistanceSquared(agent.Position);
			if (disSq < minSq)
			{
				minSq = disSq;
				nearestIdx = i;
			}
		}
	}

	if (hasPistol && !hasOneInSight && grabRangeIdx != -1)
		nearestEnemy = m_Enemies[grabRangeIdx].Location;
	else
		nearestEnemy = m_Enemies[nearestIdx].Location;

	center /= float(m_Enemies.size());
	m_pBlackboard->ChangeData(bb_EnemyInSight, true);
	m_pBlackboard->ChangeData(bb_EnemyInRange, grabRangeIdx != -1);
	m_pBlackboard->ChangeData(bb_IsInCombat, grabRangeIdx != -1 || m_Enemies.size() > 3 || m_BittenTime > 0.f);
	m_pBlackboard->ChangeData(bb_NearestEnemy, nearestEnemy);
	m_pBlackboard->ChangeData(bb_EnemyCenter, center);
}

bool Brain::GetNearestUnknownItem(Elite::Vector2& target) const
{
	auto agentPos = m_pInterface->Agent_GetInfo().Position;
	float shortestSq = FLT_MAX;
	int idx = -1;
	for (int i = 0; i < (int)m_UnknownItems.size(); i++)
	{
		float distanceSq = (m_UnknownItems[i] - agentPos).SqrtMagnitude();
		if (distanceSq < shortestSq)
		{
			shortestSq = distanceSq;
			idx = i;
		}
	}

	if (idx == -1)
		return false;

	target = m_UnknownItems[idx];
	return true;
}

vector<EntityInfo> Brain::GetEntitiesInFOV() const
{
	vector<EntityInfo> vEntitiesInFOV = {};

	EntityInfo ei = {};
	for (int i = 0;; ++i)
	{
		if (m_pInterface->Fov_GetEntityByIndex(i, ei))
		{
			vEntitiesInFOV.push_back(ei);
			continue;
		}

		break;
	}

	return vEntitiesInFOV;
}
#pragma endregion

#pragma region BLACKBOARD ----------------------------------------------------------------------------
void Brain::InitializeBlackboard()
{
	m_pBlackboard = new Blackboard{};
	m_pBlackboard->AddData(bb_pInterface, m_pInterface);

	m_pBlackboard->AddData(bb_HasLowHealth, false);
	m_pBlackboard->AddData(bb_HasLowEnergy, false);
	m_pBlackboard->AddData(bb_HasPistol, false);
	m_pBlackboard->AddData(bb_HasFreeSlot, false);

	m_pBlackboard->AddData(bb_IsInPurgeZone, false);
	m_pBlackboard->AddData(bb_PurgeZoneInfo, PurgeZoneInfo{});
	m_pBlackboard->AddData(bb_PurgeZoneDistance, 0.f);

	m_pBlackboard->AddData(bb_HasUnknownItem, false);
	m_pBlackboard->AddData(bb_UnknownItemInArea, false);
	m_pBlackboard->AddData(bb_KnowsMedKit, false);
	m_pBlackboard->AddData(bb_KnowsPistol, false);
	m_pBlackboard->AddData(bb_KnowsFood, false);
	m_pBlackboard->AddData(bb_KnowsGarbage, false);

	m_pBlackboard->AddData(bb_KnowsHouse, false);
	m_pBlackboard->AddData(bb_IsInHouse, false);
	m_pBlackboard->AddData(bb_HouseIsExplored, false);
	m_pBlackboard->AddData(bb_NextHouseIsExplored, false);
	m_pBlackboard->AddData(bb_NearestUnexploredCorner, ZeroVector2);
	m_pBlackboard->AddData(bb_ExplorationTarget, ZeroVector2);
	m_pBlackboard->AddData(bb_HouseCloserThanExploration, false);

	m_pBlackboard->AddData(bb_IsStuck, false);
	m_pBlackboard->AddData(bb_StuckTarget, ZeroVector2);

	m_pBlackboard->AddData(bb_IsInCombat, false);
	m_pBlackboard->AddData(bb_EnemyInSight, false);
	m_pBlackboard->AddData(bb_EnemyInRange, false);
	m_pBlackboard->AddData(bb_KnewEnemy, false);

	m_pBlackboard->AddData(bb_HasFullStamina, true);
	m_pBlackboard->AddData(bb_pRunMode, &m_RunMode);
	m_pBlackboard->AddData(bb_ShouldRun, false);

	m_pBlackboard->AddData(bb_ppMovementBehavior, (SteeringBehavior**)&(m_pMovement));
	m_pBlackboard->AddData(bb_pSeek, m_pSeek);
	m_pBlackboard->AddData(bb_pFlee, m_pFlee);
	m_pBlackboard->AddData(bb_MovementTarget, ZeroVector2);

	m_pBlackboard->AddData(bb_ppOrientationBehavior, &m_pOrientation);
	m_pBlackboard->AddData(bb_pForwardScanning, m_pForwardScanning);
	m_pBlackboard->AddData(bb_pFullScanning, m_pFullScanning);
	m_pBlackboard->AddData(bb_pRotateIntoFront, m_pRotateIntoFront);
	m_pBlackboard->AddData(bb_pRotateIntoVision, m_pRotateIntoVision);
	m_pBlackboard->AddData(bb_OrientationTarget, ZeroVector2);

	// targets
	m_pBlackboard->AddData(bb_HouseLocation, ZeroVector2);
	m_pBlackboard->AddData(bb_NearestItem, eItemType::RANDOM_DROP);
	m_pBlackboard->AddData(bb_NearestUnknownItem, ZeroVector2);
	m_pBlackboard->AddData(bb_NearestMedKit, ZeroVector2);
	m_pBlackboard->AddData(bb_NearestPistol, ZeroVector2);
	m_pBlackboard->AddData(bb_NearestFood, ZeroVector2);
	m_pBlackboard->AddData(bb_NearestGarbage, ZeroVector2);
	m_pBlackboard->AddData(bb_NearestEnemy, ZeroVector2);
	m_pBlackboard->AddData(bb_EnemyCenter, ZeroVector2);
}

void Brain::UpdateBlackboard()
{
	const auto& agent = m_pInterface->Agent_GetInfo();

	m_pBlackboard->ChangeData(bb_HasLowHealth, agent.Health < 7.f);
	m_pBlackboard->ChangeData(bb_HasLowEnergy, agent.Energy < 1.f);

	if (m_HasFullStamina)
		m_HasFullStamina = agent.Stamina > 9.f;
	else
		m_HasFullStamina = agent.Stamina > 9.9f;

	m_pBlackboard->ChangeData(bb_HasFullStamina, m_HasFullStamina);

	m_pBlackboard->ChangeData(bb_HasPistol, m_pInventory->HasItemOfType(eItemType::PISTOL));
	m_pBlackboard->ChangeData(bb_HasFreeSlot, m_pInventory->HasItemOfType(eItemType::RANDOM_DROP));
}

void Brain::CleanBlackboard()
{
	m_pBlackboard = new Blackboard{};
	m_pBlackboard->ChangeData(bb_pInterface, nullptr);

	m_pBlackboard->ChangeData(bb_pRunMode, nullptr);

	m_pBlackboard->ChangeData(bb_ppMovementBehavior, nullptr);
	m_pBlackboard->ChangeData(bb_pSeek, nullptr);
	m_pBlackboard->ChangeData(bb_pFlee, nullptr);

	m_pBlackboard->ChangeData(bb_ppOrientationBehavior, nullptr);
	m_pBlackboard->ChangeData(bb_pForwardScanning, nullptr);
	m_pBlackboard->ChangeData(bb_pFullScanning, nullptr);
	m_pBlackboard->ChangeData(bb_pRotateIntoFront, nullptr);
	m_pBlackboard->ChangeData(bb_pRotateIntoVision, nullptr);
}
#pragma endregion

void Brain::InitializeBehaviorTree()
{
	InitializeBlackboard();
	m_pBehaviorTree = new BehaviorTree{ m_pBlackboard, new BehaviorSequence
	{{
		new BehaviorSelector // movement --------------------------------------------
		{{
			new BehaviorSequence // purge zone
			{{
				new BehaviorConditional{IsInPurgeZone},
				new BehaviorAction{SetRunMode},
				new BehaviorAction{SetTargetPurgeZoneEscape},
				new BehaviorAction{SetSeek}
			}},

			new BehaviorSequence // combat
			{{
				new BehaviorSelector
				{{
					new BehaviorConditional{IsInCombat},
					new BehaviorConditional{EnemyInRange},
				}},

				new BehaviorSelector
				{{
					new BehaviorSequence
					{{
						new BehaviorConditional{HasPistol},
						new BehaviorSelector
						{{
							new BehaviorConditional{KnewEnemy},
							new BehaviorAction{InitializeFlee},
						}},
						new BehaviorAction{SetMovementEnemyCenter},
						new BehaviorAction{SetFlee},
					}},

					new BehaviorSequence
					{{
						new BehaviorAction{SetRunMode},
						new BehaviorConditional{FalseCondition},
					}},
				}},
			}},

			new BehaviorSequence // unknown item in sight
			{{
				new BehaviorConditional{UnknownItemInSight},
				new BehaviorAction{SetMovementUnknownItem},
				new BehaviorAction{SetSeek}
			}},

			new BehaviorSequence // unstuck
			{{
				new BehaviorConditional{IsStuck},
				new BehaviorAction{SetRunMode},
				new BehaviorAction{SetMovementExplore},
				new BehaviorAction{SetSeek}
			}},

			new BehaviorSequence // healing
			{{
				new BehaviorConditional{HasLowHealth},
				new BehaviorConditional{KnowsMedKit},
				new BehaviorAction{SetMovementMedKit},
				new BehaviorConditional{TargetCloserThanHouse},
				new BehaviorAction{SetSeek},
			}},

			new BehaviorSequence // food
			{{
				new BehaviorConditional{HasNotLowHealth},
				new BehaviorConditional{HasLowEnergy},
				new BehaviorConditional{KnowsFood},
				new BehaviorAction{SetMovementFood},
				new BehaviorConditional{TargetCloserThanHouse},
				new BehaviorAction{SetSeek}
			}},

			new BehaviorSequence // pistol
			{{
				new BehaviorConditional{HasNotLowHealth},
				new BehaviorConditional{HasNotLowEnergy},
				new BehaviorConditional{HasNoPistol},
				new BehaviorConditional{KnowsPistol},
				new BehaviorAction{SetMovementPistol},
				new BehaviorConditional{TargetCloserThanHouse},
				new BehaviorAction{SetSeek}
			}},

			new BehaviorSequence // explore house
			{{
				new BehaviorConditional{IsInHouse},
				new BehaviorConditional{HouseIsNotExplored},
				new BehaviorAction{SetMovementNearestUnexploredCorner},
				new BehaviorAction{SetSeek}
			}},

			new BehaviorSequence // move to next unknown item
			{{
				new BehaviorConditional{HasUnknownItem},
				new BehaviorAction{SetMovementUnknownItem},
				new BehaviorConditional{TargetCloserThanHouse},
				new BehaviorAction{SetSeek}
			}},

		new BehaviorSequence // move to next known item
		{{
			new BehaviorConditional{HasFreeInventorySlot},
			new BehaviorConditional{HasNotLowHealth},
			new BehaviorConditional{HasNotLowEnergy},
			new BehaviorConditional{HasPistol},
			new BehaviorAction{SetMovementItem},
			new BehaviorConditional{TargetCloserThanHouse},
			new BehaviorAction{SetSeek}
		}},

		new BehaviorSequence // move to next house
		{{
			new BehaviorConditional{KnowsHouse},
			new BehaviorAction{SetMovementHouse},
			new BehaviorAction{SetSeek}
		}},

		new BehaviorSequence // towards random location
		{{
			new BehaviorAction{SetMovementExplore},
			new BehaviorAction{SetSeek}
		}},
	}},

	new BehaviorSelector // orientation -----------------------------------------------
	{{
		new BehaviorSequence // combat
		{{
			new BehaviorSelector
			{{
				new BehaviorConditional{IsInCombat},
				new BehaviorConditional{EnemyInSight},
			}},
			new BehaviorConditional{HasPistol},
			new BehaviorAction{SetOrientationNearestEnemy},
			new BehaviorAction{SetRotateIntoFront},
		}},

		new BehaviorSequence // unknown item in sight
		{{
			new BehaviorConditional{UnknownItemInSight},
			new BehaviorAction{SetForwardScanning},
		}},

		new BehaviorSequence // stuck
		{{
			new BehaviorConditional{IsStuck},
			new BehaviorAction{SetFullScanning},
		}},

		new BehaviorAction{SetForwardScanning},
	}},

	new BehaviorSequence // extra run mode
	{{
		new BehaviorConditional{HasFullStamina},
		new BehaviorAction{SetRunMode},
	}},
}} };
}