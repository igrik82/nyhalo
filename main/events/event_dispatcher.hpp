#pragma once

#include <cstddef>

#include "event_types.hpp"

namespace app {

// Тип обработчика события
using EventHandler = void (*)(const Event &event, void *context);

// Запись в таблице подписок
struct EventSubscription {
    EventType type;
    EventHandler handler;
    void *context;
};

// Диспетчер событий с фиксированной таблицей подписок
// Размер таблицы берётся из конфига
class EventDispatcher {
public:
    static bool init();

    // Регистрация обработчика для типа события
    static bool subscribe(EventType type, EventHandler handler, void *context);

    // Обработка одного события (вызывается из задачи диспетчера)
    static void dispatch(const Event &event);

private:
    // Максимальный размер таблицы - из конфига
    // Выделяем память статически на этапе компиляции
    static constexpr size_t kMaxSubscriptions = 32;

    static EventSubscription subscriptions_[kMaxSubscriptions];
    static size_t subscription_count_;
};

}  // namespace app
