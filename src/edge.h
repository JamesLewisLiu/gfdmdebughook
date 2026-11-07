#pragma once
namespace debughook {
struct Edge {
    bool held=true;
    bool update(bool down,bool focused) {
        if(!focused){held=true;return false;}
        bool fire=down&&!held;
        held=down;
        return fire;
    }
};
}
