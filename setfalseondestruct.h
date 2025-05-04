#ifndef SETFALSEONDESTRUCT_H
#define SETFALSEONDESTRUCT_H

class SetFalseOnDestruct {
 private:
  bool *value = nullptr;

 public:
  SetFalseOnDestruct(bool &val);
  ~SetFalseOnDestruct();
};

#endif  // SETFALSEONDESTRUCT_H
