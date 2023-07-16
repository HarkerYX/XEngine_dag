/***************************
@Author: Xhosa-LEE
@Contact: lixiaoxmm@163.com
@Time: 2023/03/19
***************************/
#define CATCH_CONFIG_MAIN
#include <memory>

 #include "hpcs/graph/matrix.hpp"
#include "hpcs/common/catch.hpp"

TEST_CASE("test_matrix") {
  XEngine::Matrix<int> test_Matrix2;
  test_Matrix2.AppendCol(1);
  test_Matrix2.AppendRow(1);
}
