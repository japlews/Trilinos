// Copyright(C) 1999-2020 National Technology & Engineering Solutions
// of Sandia, LLC (NTESS).  Under the terms of Contract DE-NA0003525 with
// NTESS, the U.S. Government retains certain rights in this software.
//
// See packages/seacas/LICENSE for details

#include "SL_tokenize.h"
#include <algorithm>

std::vector<std::string> SLIB::tokenize(const std::string &str, const std::string &separators,
                                        bool allow_empty_token)
{
  std::vector<std::string> tokens;
  auto                     first = std::begin(str);
  while (first != std::end(str)) {
    const auto second =
        std::find_first_of(first, std::end(str), std::begin(separators), std::end(separators));
    if (first != second || allow_empty_token) {
      tokens.emplace_back(first, second);
    }
    if (second == std::end(str)) {
      break;
    }
    first = std::next(second);
  }
  return tokens;
}
