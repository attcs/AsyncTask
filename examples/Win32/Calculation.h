#pragma once

#include <vector>
#include <algorithm>
#include <execution>

using Matrix = std::vector<std::vector<double>>;

Matrix matrix_random(size_t N, size_t M)
{
  auto m = Matrix(N);
  std::for_each(std::execution::par, m.begin(), m.end(), [M](auto& row)
  {
    row.resize(M);
    std::for_each(row.begin(), row.end(), [](auto& e) { e = rand(); });
  });

  return m;
}

double matrix_product_element(Matrix const& m1, Matrix const& m2, size_t i, size_t j)
{
  auto const O = m2.size();
  if (m1.size() == 0 || O == 0)
    return 0.0;

  assert(m1[i].size() == O);

  auto r = 0.0;
  for (size_t id = 0; id < O; ++id)
    r += m1[i][id] * m2[id][j];

  return r;
}

