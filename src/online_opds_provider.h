#pragma once

#include "online_source_types.h"

#include <string>
#include <vector>

std::vector<OnlineCatalogItem> ParseOpdsCatalog(const std::string &xml, const std::string &base_url);
