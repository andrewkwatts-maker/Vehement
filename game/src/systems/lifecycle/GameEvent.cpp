#include "GameEvent.hpp"
#include <algorithm>
#include <mutex>

namespace Vehement {
namespace Lifecycle {

// ============================================================================
// EventTypeToString Implementation
// ============================================================================

const char* EventTypeToString(EventType type) {
    switch (type) {
        // Lifecycle events
        case EventType::None:               return "None";
        case EventType::Spawned:            return "Spawned";
        case EventType::Destroyed:          return "Destroyed";
        case EventType::Activated:          return "Activated";
        case EventType::Deactivated:        return "Deactivated";
        case EventType::Paused:             return "Paused";
        case EventType::Resumed:            return "Resumed";
        case EventType::StateChanged:       return "StateChanged";
        case EventType::EnabledChanged:     return "EnabledChanged";
        case EventType::VisibilityChanged:  return "VisibilityChanged";

        // Combat events
        case EventType::CombatStart:        return "CombatStart";
        case EventType::Damaged:            return "Damaged";
        case EventType::Healed:             return "Healed";
        case EventType::Killed:             return "Killed";
        case EventType::Revived:            return "Revived";
        case EventType::AttackStarted:      return "AttackStarted";
        case EventType::AttackLanded:       return "AttackLanded";
        case EventType::AttackMissed:       return "AttackMissed";
        case EventType::AttackBlocked:      return "AttackBlocked";
        case EventType::StatusApplied:      return "StatusApplied";
        case EventType::StatusRemoved:      return "StatusRemoved";
        case EventType::StatusTick:         return "StatusTick";
        case EventType::CriticalHit:        return "CriticalHit";
        case EventType::DodgedAttack:       return "DodgedAttack";
        case EventType::ShieldBroken:       return "ShieldBroken";

        // Building events
        case EventType::BuildingStart:      return "BuildingStart";
        case EventType::Built:              return "Built";
        case EventType::ConstructionStarted:return "ConstructionStarted";
        case EventType::ConstructionProgress:return "ConstructionProgress";
        case EventType::Demolished:         return "Demolished";
        case EventType::Upgraded:           return "Upgraded";
        case EventType::UpgradeStarted:     return "UpgradeStarted";
        case EventType::UpgradeProgress:    return "UpgradeProgress";
        case EventType::UpgradeCancelled:   return "UpgradeCancelled";
        case EventType::ProductionStarted:  return "ProductionStarted";
        case EventType::ProductionComplete: return "ProductionComplete";
        case EventType::ProductionCancelled:return "ProductionCancelled";
        case EventType::ProductionQueued:   return "ProductionQueued";
        case EventType::GarrisonEntered:    return "GarrisonEntered";
        case EventType::GarrisonExited:     return "GarrisonExited";
        case EventType::GateOpened:         return "GateOpened";
        case EventType::GateClosed:         return "GateClosed";

        // Unit events
        case EventType::UnitStart:          return "UnitStart";
        case EventType::MovementStarted:    return "MovementStarted";
        case EventType::MovementStopped:    return "MovementStopped";
        case EventType::DestinationReached: return "DestinationReached";
        case EventType::PathBlocked:        return "PathBlocked";
        case EventType::TargetAcquired:     return "TargetAcquired";
        case EventType::TargetLost:         return "TargetLost";
        case EventType::TargetChanged:      return "TargetChanged";
        case EventType::OrderReceived:      return "OrderReceived";
        case EventType::OrderCompleted:     return "OrderCompleted";
        case EventType::OrderCancelled:     return "OrderCancelled";
        case EventType::GroupJoined:        return "GroupJoined";
        case EventType::GroupLeft:          return "GroupLeft";
        case EventType::FormationChanged:   return "FormationChanged";

        // Projectile events
        case EventType::ProjectileStart:    return "ProjectileStart";
        case EventType::Launched:           return "Launched";
        case EventType::ProjectileHit:      return "ProjectileHit";
        case EventType::ProjectileExpired:  return "ProjectileExpired";
        case EventType::Exploded:           return "Exploded";
        case EventType::Bounced:            return "Bounced";

        // Effect events
        case EventType::EffectStart:        return "EffectStart";
        case EventType::EffectStarted:      return "EffectStarted";
        case EventType::EffectEnded:        return "EffectEnded";
        case EventType::EffectLooped:       return "EffectLooped";
        case EventType::AbilityCast:        return "AbilityCast";
        case EventType::AbilityHit:         return "AbilityHit";
        case EventType::AbilityCancelled:   return "AbilityCancelled";
        case EventType::CooldownStarted:    return "CooldownStarted";
        case EventType::CooldownReady:      return "CooldownReady";

        // Resource events
        case EventType::ResourceStart:      return "ResourceStart";
        case EventType::ResourceGained:     return "ResourceGained";
        case EventType::ResourceSpent:      return "ResourceSpent";
        case EventType::ResourceDepleted:   return "ResourceDepleted";
        case EventType::ResourceDiscovered: return "ResourceDiscovered";

        // Game events
        case EventType::GameStart:          return "GameStart";
        case EventType::WaveStarted:        return "WaveStarted";
        case EventType::WaveCompleted:      return "WaveCompleted";
        case EventType::BossSpawned:        return "BossSpawned";
        case EventType::ObjectiveUpdated:   return "ObjectiveUpdated";
        case EventType::ObjectiveCompleted: return "ObjectiveCompleted";
        case EventType::GamePaused:         return "GamePaused";
        case EventType::GameResumed:        return "GameResumed";
        case EventType::GameOver:           return "GameOver";
        case EventType::Victory:            return "Victory";
        case EventType::Defeat:             return "Defeat";

        case EventType::CustomEventBase:    return "CustomEventBase";
        case EventType::MaxEventType:       return "MaxEventType";

        default:
            if (IsCustomEvent(type)) {
                return "CustomEvent";
            }
            return "Unknown";
    }
}

// ============================================================================
// EventDispatcher Implementation
// ============================================================================

EventDispatcher::EventDispatcher() = default;
EventDispatcher::~EventDispatcher() = default;

EventSubscription EventDispatcher::Subscribe(EventType type, EventCallback callback, int priority) {
    return Subscribe(type, std::move(callback), nullptr, priority);
}

EventSubscription EventDispatcher::Subscribe(EventType type, EventCallback callback,
                                            EventFilter filter, int priority) {
    EventSubscription sub;
    sub.id = m_nextSubscriptionId++;
    sub.type = type;

    SubscriberInfo info;
    info.id = sub.id;
    info.callback = std::move(callback);
    info.filter = std::move(filter);
    info.priority = priority;

    m_subscribers[type].push_back(std::move(info));
    SortSubscribers(type);

    return sub;
}

std::vector<EventSubscription> EventDispatcher::Subscribe(const std::vector<EventType>& types,
                                                          EventCallback callback, int priority) {
    std::vector<EventSubscription> subs;
    subs.reserve(types.size());

    for (EventType type : types) {
        subs.push_back(Subscribe(type, callback, priority));
    }

    return subs;
}

EventSubscription EventDispatcher::SubscribeToCategory(EventCallback callback,
                                                       bool(*categoryCheck)(EventType),
                                                       int priority) {
    EventSubscription sub;
    sub.id = m_nextSubscriptionId++;
    sub.type = EventType::None; // Category subscription

    CategorySubscriber info;
    info.id = sub.id;
    info.callback = std::move(callback);
    info.categoryCheck = categoryCheck;
    info.priority = priority;

    m_categorySubscribers.push_back(std::move(info));

    // Sort category subscribers by priority
    std::sort(m_categorySubscribers.begin(), m_categorySubscribers.end(),
              [](const CategorySubscriber& a, const CategorySubscriber& b) {
                  return a.priority > b.priority;
              });

    return sub;
}

void EventDispatcher::Unsubscribe(const EventSubscription& subscription) {
    if (!subscription.IsValid()) return;

    // Check type-specific subscribers
    if (subscription.type != EventType::None) {
        auto it = m_subscribers.find(subscription.type);
        if (it != m_subscribers.end()) {
            auto& subs = it->second;
            subs.erase(std::remove_if(subs.begin(), subs.end(),
                [&](const SubscriberInfo& info) {
                    return info.id == subscription.id;
                }), subs.end());
        }
    }

    // Check category subscribers
    m_categorySubscribers.erase(
        std::remove_if(m_categorySubscribers.begin(), m_categorySubscribers.end(),
            [&](const CategorySubscriber& info) {
                return info.id == subscription.id;
            }), m_categorySubscribers.end());
}

void EventDispatcher::UnsubscribeAll(EventType type) {
    m_subscribers.erase(type);
}

void EventDispatcher::ClearAllSubscriptions() {
    m_subscribers.clear();
    m_categorySubscribers.clear();
}

bool EventDispatcher::Dispatch(GameEvent& event) {
    // Check if cancelled before dispatch
    if (event.cancelled) {
        return false;
    }

    bool handled = false;

    // Call category subscribers first
    for (const auto& categorySub : m_categorySubscribers) {
        if (categorySub.categoryCheck && categorySub.categoryCheck(event.type)) {
            if (categorySub.callback(event)) {
                handled = true;
                if (event.handled) break;
            }
        }
    }

    // Then call type-specific subscribers
    if (!event.handled) {
        auto it = m_subscribers.find(event.type);
        if (it != m_subscribers.end()) {
            for (const auto& sub : it->second) {
                // Check filter
                if (sub.filter && !sub.filter(event)) {
                    continue;
                }

                // Call callback
                if (sub.callback(event)) {
                    handled = true;
                }

                // Stop if handled
                if (event.handled) {
                    break;
                }
            }
        }
    }

    return handled;
}

void EventDispatcher::QueueEvent(GameEvent event) {
    QueuedEvent qe;
    qe.event = std::move(event);
    qe.processTime = qe.event.timestamp + qe.event.delay;
    m_eventQueue.push(std::move(qe));
}

void EventDispatcher::QueueDelayedEvent(GameEvent event, float delay) {
    event.delay = delay;
    QueueEvent(std::move(event));
}

void EventDispatcher::ProcessQueuedEvents(double currentTime) {
    while (!m_eventQueue.empty()) {
        const auto& top = m_eventQueue.top();
        if (top.processTime > currentTime) {
            break; // No more events ready
        }

        GameEvent event = std::move(const_cast<QueuedEvent&>(top).event);
        m_eventQueue.pop();

        Dispatch(event);
    }
}

void EventDispatcher::ClearEventQueue() {
    while (!m_eventQueue.empty()) {
        m_eventQueue.pop();
    }
}

size_t EventDispatcher::GetSubscriberCount(EventType type) const {
    auto it = m_subscribers.find(type);
    return it != m_subscribers.end() ? it->second.size() : 0;
}

size_t EventDispatcher::GetQueuedEventCount() const {
    return m_eventQueue.size();
}

size_t EventDispatcher::GetTotalSubscriptionCount() const {
    size_t count = m_categorySubscribers.size();
    for (const auto& pair : m_subscribers) {
        count += pair.second.size();
    }
    return count;
}

void EventDispatcher::SortSubscribers(EventType type) {
    auto it = m_subscribers.find(type);
    if (it != m_subscribers.end()) {
        std::sort(it->second.begin(), it->second.end(),
                  [](const SubscriberInfo& a, const SubscriberInfo& b) {
                      return a.priority > b.priority; // Higher priority first
                  });
    }
}

// ============================================================================
// Global Event Dispatcher
// ============================================================================

namespace {
    std::unique_ptr<EventDispatcher> g_globalDispatcher;
    std::once_flag g_dispatcherInitFlag;
}

EventDispatcher& GetGlobalEventDispatcher() {
    std::call_once(g_dispatcherInitFlag, []() {
        g_globalDispatcher = std::make_unique<EventDispatcher>();
    });
    return *g_globalDispatcher;
}

} // namespace Lifecycle
} // namespace Vehement
