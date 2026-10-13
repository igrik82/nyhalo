#include "event_dispatcher.hpp"

#include "../app_config.hpp"
#include "esp_log.h"

static const char *TAG = "EVENT_DISPATCHER";

namespace app {

EventSubscription
    EventDispatcher::subscriptions_[EventDispatcher::kMaxSubscriptions];
size_t EventDispatcher::subscription_count_ = 0;

bool EventDispatcher::init() {
    const auto &config = getConfig();

    // Проверяем, что конфиг не превышает статический лимит
    if (config.dispatcher.max_subscribers > kMaxSubscriptions) {
        ESP_LOGE(TAG, "config max_subscriptions=%u exceeds static limit=%u",
                 static_cast<unsigned>(config.dispatcher.max_subscribers),
                 static_cast<unsigned>(kMaxSubscriptions));
        return false;
    }

    subscription_count_ = 0;
    ESP_LOGI(TAG, "event dispatcher initialized (max_subscriptions=%u)",
             static_cast<unsigned>(config.dispatcher.max_subscribers));
    return true;
}

bool EventDispatcher::subscribe(EventType type, EventHandler handler,
                                void *context) {
    const auto &config = getConfig();

    if (subscription_count_ >= config.dispatcher.max_subscribers) {
        ESP_LOGE(TAG, "subscription table full (%u/%u)",
                 static_cast<unsigned>(subscription_count_),
                 static_cast<unsigned>(config.dispatcher.max_subscribers));
        return false;
    }

    subscriptions_[subscription_count_].type = type;
    subscriptions_[subscription_count_].handler = handler;
    subscriptions_[subscription_count_].context = context;
    ++subscription_count_;

    ESP_LOGD(TAG, "subscribed to event: %s (%u/%u)", eventTypeToString(type),
             static_cast<unsigned>(subscription_count_),
             static_cast<unsigned>(config.dispatcher.max_subscribers));
    return true;
}

void EventDispatcher::dispatch(const Event &event) {
    ESP_LOGD(TAG, "dispatching event: %s (timestamp=%lu)",
             eventTypeToString(event.type),
             static_cast<unsigned long>(event.timestamp));

    for (size_t i = 0; i < subscription_count_; ++i) {
        if (subscriptions_[i].type == event.type) {
            if (subscriptions_[i].handler != nullptr) {
                subscriptions_[i].handler(event, subscriptions_[i].context);
            }
        }
    }
}

}  // namespace app
