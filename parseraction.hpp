#ifndef PARSERACTION_HPP
#define PARSERACTION_HPP

namespace Parser {
  class Action
  {
  public:
    virtual ~Action() {}
  };

  class Ignore : public Action {};
  class Print : public Action {};
  class Execute : public Action {};
  class Clear : public Action {};
  class Collect : public Action {};
  class Param : public Action {};
  class ESC_Dispatch : public Action {};
  class CSI_Dispatch : public Action {};
  class Hook : public Action {};
  class Put : public Action {};
  class Unhook : public Action {};
  class OSC_Start : public Action {};
  class OSC_Put : public Action {};
  class OSC_End : public Action {};
}

#endif
