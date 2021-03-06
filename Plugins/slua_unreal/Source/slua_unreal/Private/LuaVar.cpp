// The MIT License (MIT)

// Copyright 2015 Siney/Pangweiwei siney@yeah.net
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "LuaVar.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "UObject/Stack.h"
#include "Blueprint/WidgetTree.h"
#include "LuaState.h"

namespace slua {

    LuaVar::LuaVar()
        :L(nullptr)
    {
        vars = nullptr;
        numOfVar = 0;
    }

    LuaVar::LuaVar(lua_Integer v)
        :LuaVar()
    {
        set(v);
    }

    LuaVar::LuaVar(int v)
        :LuaVar()
    {
        set((lua_Integer)v);
    }

    LuaVar::LuaVar(size_t v)
        :LuaVar()
    {
        set((lua_Integer)v);
    }

    LuaVar::LuaVar(lua_Number v)
        :LuaVar()
    {
        set(v);
    }

    LuaVar::LuaVar(bool v)
        :LuaVar()
    {
        set(v);
    }

    LuaVar::LuaVar(const char* v)
        :LuaVar()
    {
        set(v);
    }

    LuaVar::LuaVar(lua_State* l,int p):LuaVar() {
        set(l,p);
    }

    void LuaVar::set(lua_State* l,int p) {
        free();
        int t = lua_type(l,p);
        LuaVar::Type type = LV_NIL;
        switch(t) {
            case LUA_TNUMBER:
                {
                if(lua_isinteger(l,p))
                    type = LV_INT;
                else
                    type = LV_NUMBER;
                }
                break;
            case LUA_TSTRING:
                type = LV_STRING;
                break;
            case LUA_TFUNCTION:
                type = LV_FUNCTION;
                break;
            case LUA_TTABLE:
                type = LV_TABLE;
                break;
            case LUA_TUSERDATA:
                type = LV_USERDATA;
                break;
            case LUA_TNIL:
            default:
                type = LV_NIL;
                break;
        }
        init(l,p,type);
    }

    LuaVar::LuaVar(lua_State* l,int p,LuaVar::Type type):LuaVar() {
        init(l,p,type);
    }

    // used to create number n of tuple
    // it used for return value from lua
    // don't call it to create n element of tuple
    LuaVar::LuaVar(lua_State* l,size_t n):LuaVar() {
        init(l,n,LV_TUPLE);
    }

    void LuaVar::init(lua_State* l,int p,LuaVar::Type type) {
        switch(type) {
        case LV_NIL:
            break;
        case LV_INT:
            set(lua_tointeger(l,p));
            break;
        case LV_NUMBER:
            set(lua_tonumber(l,p));
            break;
        case LV_STRING:
            set(lua_tostring(l,p));
            break;
        case LV_FUNCTION: 
        case LV_TABLE:
        case LV_USERDATA:
            this->L = l;
            alloc(1);
            lua_pushvalue(l,p);
            vars[0].ref = new RefRef(l);
            vars[0].luatype=type;
            break;
        case LV_TUPLE:
            this->L = l;
            ensure(p>0 && lua_gettop(L)>=p);
            initTuple(p);
            break;
        default:
            break;
        }
    }

    void LuaVar::initTuple(size_t n) {
        ensure(lua_gettop(L)>=n);
        alloc(n);
        int f = lua_gettop(L)-n+1;
        for(int i=0;i<n;i++) {
            
            int p = i+f;
            int t = lua_type(L,p);

            switch(t) {
            case LUA_TNUMBER:
                {
                    if(lua_isinteger(L,p)) {
                        vars[i].luatype = LV_INT;
                        vars[i].i = lua_tointeger(L,p);
                    }
                    else {
                        vars[i].luatype = LV_NUMBER;
                        vars[i].d = lua_tonumber(L,p);
                    }
                }
                break;
            case LUA_TSTRING:
                vars[i].luatype = LV_STRING;
                vars[i].s = new RefStr(lua_tostring(L,p));
                break;
            case LUA_TFUNCTION:
                vars[i].luatype = LV_FUNCTION;
                lua_pushvalue(L,p);
                vars[i].ref = new RefRef(L);
                break;
            case LUA_TTABLE:
                vars[i].luatype = LV_TABLE;
                lua_pushvalue(L,p);
                vars[i].ref = new RefRef(L);
                break;
            case LUA_TNIL:
            default:
                vars[i].luatype = LV_NIL;
                break;
            }
        }
    }

    LuaVar::~LuaVar() {
        free();
    }

    void LuaVar::free() {
        for(int n=0;n<numOfVar;n++) {
            if( (vars[n].luatype==LV_FUNCTION || vars[n].luatype==LV_TABLE) 
                && vars[n].ref->isValid() )
                vars[n].ref->release();
            else if(vars[n].luatype==LV_STRING)
                vars[n].s->release();
        }
        numOfVar = 0;
        delete[] vars;
        vars = nullptr;
    }

    void LuaVar::alloc(int n) {
        if(n>0) {
            vars = new lua_var[n];
            numOfVar = n;
        }
    }

    size_t LuaVar::count() const {
        if(isTable()) {
            push(L);
            size_t n = lua_rawlen(L,-1);
            lua_pop(L,1);
            return n;
        }
        return numOfVar;
    }

    int LuaVar::asInt() const {
        ensure(numOfVar==1);
        switch(vars[0].luatype) {
        case LV_INT:
            return vars[0].i;
        case LV_NUMBER:
            return vars[0].d;
        default:
            return -1;
        }
    }

    float LuaVar::asFloat() const {
        ensure(numOfVar==1);
        switch(vars[0].luatype) {
        case LV_INT:
            return vars[0].i;
        case LV_NUMBER:
            return vars[0].d;
        default:
            return NAN;
        }
    }

    double LuaVar::asDouble() const {
        ensure(numOfVar==1);
        switch(vars[0].luatype) {
        case LV_INT:
            return vars[0].i;
        case LV_NUMBER:
            return vars[0].d;
        default:
            return NAN;
        }
    }

    const char* LuaVar::asString() const {
        ensure(numOfVar==1 && vars[0].luatype==LV_STRING);
        return vars[0].s->str;
    }

    bool LuaVar::asBool() const {
        ensure(numOfVar==1 && vars[0].luatype==LV_BOOL);
        return vars[0].b;
    }



    LuaVar LuaVar::getAt(size_t index) const {
        if(isTable()) {
            push(L); // push this table
            lua_geti(L,-1,index); // get by index
            LuaVar r(L,-1); // construct LuaVar
            lua_pop(L,2); // pop table and value
            return r;
        }
        else {
            ensure(index>0);
            ensure(numOfVar>=index);
            LuaVar r;
            r.alloc(1);
            r.L = this->L;
            varClone(r.vars[0],vars[index-1]);
            return r;
        }
    }

    LuaVar LuaVar::getFromTable(const LuaVar& key) const {
        ensure(isTable());
        push(L);
        key.push(L);
        lua_gettable(L,-2);
        LuaVar r(L,-1);
        lua_pop(L,2);
        return r;
    }

    void LuaVar::set(lua_Integer v) {
        free();
        alloc(1);
        vars[0].i = v;
        vars[0].luatype = LV_INT;
    }

    void LuaVar::set(int v) {
        free();
        alloc(1);
        vars[0].i = v;
        vars[0].luatype = LV_INT;
    }

    void LuaVar::set(lua_Number v) {
        free();
        alloc(1);
        vars[0].d = v;
        vars[0].luatype = LV_NUMBER;
    }

    void LuaVar::set(const char* v) {
        free();
        alloc(1);
        vars[0].s = new RefStr(v);
        vars[0].luatype = LV_STRING;
    }

    void LuaVar::set(bool b) {
        free();
        alloc(1);
        vars[0].b = b;
        vars[0].luatype = LV_BOOL;
    }

    void LuaVar::pushVar(lua_State* l,const lua_var& ov) const {
        switch(ov.luatype) {
        case LV_INT:
            lua_pushinteger(l,ov.i);
            break;
        case LV_NUMBER:
            lua_pushnumber(l,ov.d);
            break;
        case LV_BOOL:
            lua_pushboolean(l,ov.b);
            break;
        case LV_STRING:
            lua_pushstring(l,ov.s->str);
            break;
        case LV_FUNCTION:
        case LV_TABLE:
        case LV_USERDATA:
            ov.ref->push(l);
            break;
        default:
            lua_pushnil(l);
            break;
        }
    }

    int LuaVar::push(lua_State* l) const {
        if(l==nullptr) l=L;
        if(l==nullptr) return 0;

        if(vars==nullptr || numOfVar==0) {
            lua_pushnil(l);
            return 1;
        }
        
        if(numOfVar==1) {
            const lua_var& ov = vars[0];
            pushVar(l,ov);
            return 1;
        }
        else for(int n=0;n<numOfVar;n++) {
            const lua_var& ov = vars[n];
            pushVar(l,ov);
            return numOfVar;
        }
        return 0;
    }

    bool LuaVar::isNil() const {
        return vars==nullptr || numOfVar==0;
    }

    bool LuaVar::isFunction() const {
        return numOfVar==1 && vars[0].luatype==LV_FUNCTION;
    }

    bool LuaVar::isTuple() const {
        return numOfVar>1;
    }

    bool LuaVar::isTable() const {
        return numOfVar==1 && vars[0].luatype==LV_TABLE;
    }

    bool LuaVar::isInt() const {
        return numOfVar==1 && vars[0].luatype==LV_INT;
    }

    bool LuaVar::isNumber() const {
        return numOfVar==1 && vars[0].luatype==LV_NUMBER;
    }

    bool LuaVar::isBool() const {
        return numOfVar==1 && vars[0].luatype==LV_BOOL;
    }

    bool LuaVar::isUserdata(const char* t) const {
        if(numOfVar==1 && vars[0].luatype==LV_USERDATA) {
            push(L);
            void* p = luaL_testudata(L, -1, t);
            lua_pop(L,1);
            return p!=nullptr;
        }
        return false;
    }

    bool LuaVar::isString() const {
        return numOfVar==1 && vars[0].luatype==LV_STRING;
    }

    LuaVar::Type LuaVar::type() const {
        if(numOfVar==0)
            return LV_NIL;
        else if(numOfVar==1)
            return vars[0].luatype;
        else
            return LV_TUPLE;
    }

    int LuaVar::docall(int argn) {
        int top = lua_gettop(L);
        top=top-argn+1;
        LuaState::pushErrorHandler(L);
        lua_insert(L,top);
        vars[0].ref->push(L);
        lua_insert(L,top+1);
        // top is err handler
        if(lua_pcallk(L,argn,LUA_MULTRET,top,NULL,NULL))
            lua_pop(L,1);
        lua_remove(L,top); // remove err handler;
        return lua_gettop(L)-top+1;
    }

    int LuaVar::pushArgByParms(UProperty* prop,uint8* parms) {
        if (LuaObject::push(L,prop,parms))
            return prop->ElementSize;
        return 0;
    }

    void LuaVar::callByUFunction(UFunction* func,uint8* parms) {
        int n=0;
        for(TFieldIterator<UProperty> it(func);it && (it->PropertyFlags&CPF_Parm);++it) {
            UProperty* prop = *it;
            uint64 propflag = prop->GetPropertyFlags();
            if((propflag&CPF_ReturnParm))
                continue;

            pushArgByParms(prop,parms+prop->GetOffset_ForInternal());
            n++;
        }
        docall(n);
    }

    void LuaVar::varClone(lua_var& tv,const lua_var& ov) const {
        switch(ov.luatype) {
        case LV_INT:
            tv.i = ov.i;
            break;
        case LV_NUMBER:
            tv.d = ov.d;
            break;
        case LV_STRING:
            tv.s = ov.s;
            tv.s->addRef();
            break;
        case LV_FUNCTION:
        case LV_TABLE:
        case LV_USERDATA:
            tv.ref = ov.ref;
            tv.ref->addRef();
            break;
        }
        tv.luatype = ov.luatype;
    }

    void LuaVar::clone(const LuaVar& other) {
        L = other.L;
        numOfVar = other.numOfVar;
        if(numOfVar>0 && other.vars) {
            vars = new lua_var[numOfVar];
            for(int n=0;n<numOfVar;n++) {
                varClone( vars[n], other.vars[n] );
            }
        }
    }

    void LuaVar::move(LuaVar&& other) {
        L = other.L;
        numOfVar = other.numOfVar;
        vars = other.vars;

        other.numOfVar = 0;
        other.vars = nullptr;
    }

    typedef int (*CheckPropertyFunction)(lua_State* L,UProperty* prop,uint8* parms,int i);
    CheckPropertyFunction getChecker(UClass* cls);

    bool LuaVar::toProperty(UProperty* p,uint8* ptr) {

        auto func = slua::getChecker(p->GetClass());
        if(func) {
            // push var's value to top of stack
            push(L);
            bool ok = (*func)(L,p,ptr,lua_absindex(L,-1));
            lua_pop(L,1);
            return ok;
        }
        
        return false;
    }
}
