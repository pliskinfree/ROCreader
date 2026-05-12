#pragma once

#include "book_scanner.h"
#include "online_source_runtime.h"

#include <functional>
#include <string>
#include <vector>

bool MarkOnlineItemForLocal(OnlineSourceState &state, const BookItem &item);
bool UnmarkOnlineItemForLocal(OnlineSourceState &state, const BookItem &item);
bool OnlineItemMarkedForLocal(const OnlineSourceState &state, const BookItem &item);
std::vector<BookItem> BuildMarkedOnlineShelfItems(const OnlineSourceState &state);
size_t CountMarkedOnlineItems(const OnlineSourceState &state);

struct OnlineMigrationResult {
  size_t total = 0;
  size_t moved = 0;
  size_t failed = 0;
};

OnlineMigrationResult MigrateMarkedOnlineItems(
    OnlineSourceState &state,
    const std::vector<std::string> &books_roots,
    const std::vector<std::string> &cover_roots,
    const std::function<void(size_t, size_t)> &on_progress = {});

void DisconnectOnlineSourceWithMigration(OnlineSourceState &state,
                                         const std::vector<std::string> &books_roots,
                                         const std::vector<std::string> &cover_roots);
