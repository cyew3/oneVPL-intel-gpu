#include "msdk_ts.h"
#include "test_trace.h"

using namespace msdk_ts;

typedef ts* test_hdl;

void* new_var(test_hdl hdl, const char* key, unsigned long size){
    try{
        return hdl->new_var(key, size);
    }catch(test_result){
        return NULL;
    }
}

void* get_var(test_hdl hdl, const char* key, unsigned long offset){
    try{
        return ((char*)hdl->get_var(key) + offset);
    }catch(test_result){
        return NULL;
    }
}

void* set_var(test_hdl hdl, const char* key, void* val){
    try{
        return hdl->set_var(key, val);
    }catch(test_result){
        return NULL;
    }
}

test_result ts::run_block(const char* name) { 
    block_map_type::const_iterator it = m_block.find(key(name));
    test_result res = resUNKBLK;
    _break_cond& b = var_def<_break_cond>("break_cond", &ts::repeat_0);

    std::cout << "BLOCK: " << name << "\n";

    if(it != m_block.end()){ 
        b = &ts::repeat_0;
        try {
            //load_param(in, ++m_level);
            do{
                res = (this->*it->second)();
                if(res == resFAIL && var_def<bool>("sb_delayed_fail", false)){
                    var_old_or_new<bool>("sb_failed") = true;
                    std::cout << "--FAILED-- " << std::endl;
                    res = resOK;
                }else{
                    if(res) break;
                    std::cout << "--OK-- " << std::endl;
                }
            }while(!(this->*b)());
            m_level--;
        } catch(test_result e){
            res = e;
        }
    }
    return res;
};

int run_block(test_hdl hdl, const char* name){
    return (int)hdl->run_block(name);
}

test_hdl init_test(){
    allow_debug_output();
    std::cout << "INIT TEST\n";
    return new ts(std::cin);
}

void close_test(test_hdl hdl){
    std::cout << "CLOSE TEST\n";
    delete hdl;
}
