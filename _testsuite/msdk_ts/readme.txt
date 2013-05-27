
0) usage: msdk_ts[.exe] < par_file.par

1) .par file format.
<block_name>
    #comment
    <operation> #indent size is exactly 4 spaces
    ...
<block_name>
...
loop
    #default pre-defined block
    loop_break v= cf_<break_func>
    #^one of predefined functions (cf_less,cf_cnt), default is repeat_0
    [break_func and global loop operations]
        <block_name>
            <operation>
        ...

2) Operations
    All operations will be processed before block start.
    Operation format:
        <var_name>[+<offset>]  <operator> <var_value>
    Operators:
        1= - set 1-byte value, <var_value> - digit
        2= - set 2-byte value, <var_value> - digit
        4= - set 4-byte value, <var_value> - digit
        p= - set sizeof(void*)-byte value, <var_value> - digit (usefull for null-pointers)
        s= - string variable, <var_value> - string(no spaces), offset ignored
        r= - make reference, <var_value> - other variable
        v= - copy variable context(make alias), <var_value> - other variable, offset ignored
        b= - allocate buffer, <var_value> - buffer size (4-byte digit)
    To avoid var. name conflicts, use "u_" prefix for user defined variables(not connected directly with any block)

3) Block prefixes
    b_  - base block(block over MFX function), location: src/test_blocks.cpp
    t_  - tool (non-testing support block like: set parameters, memory allocation, loop control etc.), location: src/test_tools.cpp
    v_  - verification block, location: src/test_verif.cpp

4) Add block
    a. Declare block in include/msdk_ts_blocks:
        msdk_ts_DECLARE_BLOCK(<block_name>);
    b. Implement block:
        msdk_ts_BLOCK(<block_name>){
            
            ...
            
            return msdk_ts::resOK; // return value is msdk_ts::test_result
        }
    c. Locate your block according type (base, tool, verif)

5) Block variables
    <key> - variable name, used in .par file
    type& var = var_old<type>("<key>", offset = 0); // throw error if variable was not defined
    type& var = var_new<type>("<key>"); // new variable
	type& var = var_new<type>("<key>", new MyClass); // new variable with predefined value, deleted automatically
    type& var = var_old_or_new<type>("<key>"); // new if not defined
    type& var = var_def<type>("<key>", <default_val>); // if not defined, new var. withvalue <default_val>

6) Loop control in block
    use return codes loopCONTINUE and loopBREAK


