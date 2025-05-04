#include "setfalseondestruct.h"

SetFalseOnDestruct::SetFalseOnDestruct(bool &val) : value(&val) {}

SetFalseOnDestruct::~SetFalseOnDestruct()
{
  *value = false;
}
