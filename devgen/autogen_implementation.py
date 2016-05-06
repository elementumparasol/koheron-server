# Generate the implementation template for the device
#
# (c) Koheron

import os
import device as dev_utils

def Generate(device, directory):
    filename = os.path.join(directory, device.class_name.lower() + '.cpp')
    f = open(filename, 'w')
        
    try:      
        PrintFileHeader(f, os.path.basename(filename))
        
        f.write('#include "' + device.class_name.lower() + '.hpp' + '"\n\n')
        
        f.write('#include "../core/commands.hpp"\n')
        f.write('#include "../core/kserver.hpp"\n')
        f.write('#include "../core/kserver_session.hpp"\n')
        #f.write('#include "../core/binary_parser.hpp"\n\n')
        
        f.write('namespace kserver {\n\n')
        
        f.write("#define THIS (static_cast<" + device.class_name + "*>(this))\n\n")
        
        for operation in device.operations:
            f.write('/////////////////////////////////////\n')
            f.write('// ' + operation["name"] + '\n\n')
            
            PrintParseArg(f, device, operation)
            PrintExecuteOp(f, device, operation)
        
        PrintIsFailed(f, device)
        PrintExecute(f, device)
           
        f.write('} // namespace kserver\n\n')
        
        f.close()
    except:
        f.close()
        os.remove(filename)
        raise
    
def PrintFileHeader(file_id, filename):
    file_id.write('/// ' + filename + '\n')
    file_id.write('///\n')
    file_id.write('/// Generated by devgen. \n')
    file_id.write('/// DO NOT EDIT. \n')
    file_id.write('///\n')
    file_id.write('/// (c) Koheron \n\n')
    
# -----------------------------------------------------------
# PrintParseArg:
# Autogenerate the parser
# -----------------------------------------------------------
    
def PrintParseArg(file_id, device, operation):
    file_id.write('template<>\n')
    file_id.write('template<>\n')
    
    file_id.write('int KDevice<' + device.class_name + ',' + device.name + '>::\n')
    file_id.write('        parse_arg<' + device.class_name + '::' \
                    + operation["name"] + '> (const Command& cmd,\n' )
    file_id.write('                KDevice<' + device.class_name + ',' \
                        + device.name + '>::\n')
    file_id.write('                Argument<' + device.class_name \
                    + '::' + operation["name"] + '>& args)\n' )
    file_id.write('{\n')
    
    try:
        PrintParserCore(file_id, device, operation)
    except TypeError:
        raise
        
    file_id.write('    return 0;\n')
    file_id.write('}\n\n')
    
def PrintParserCore(file_id, device, operation):    
    if GetTotalArgNum(operation) == 0:
        return

    file_id.write('    if (required_buffer_size<')
    PrintTypeList(file_id, operation)
    file_id.write('>() != cmd.payload_size) {\n')
    file_id.write("        kserver->syslog.print(SysLog::ERROR, \"Invalid payload size\\n\");\n")
    file_id.write("        return -1;\n")
    file_id.write("    }\n\n")

    file_id.write('    auto args_tuple = parse_buffer<0, ')
    PrintTypeList(file_id, operation)
    file_id.write('>(cmd.buffer);\n')

    for idx, arg in enumerate(operation["arguments"]):
        file_id.write('    args.' + arg["name"] + ' = ' + 'std::get<' + str(idx) + '>(args_tuple);\n');

def PrintTypeList(file_id, operation):
    for idx, arg in enumerate(operation["arguments"]):
        if idx < GetTotalArgNum(operation) - 1:   
            file_id.write(arg["type"] + ',')
        else:
            file_id.write(arg["type"])
    
def GetTotalArgNum(operation):
    if not dev_utils.IsArgs(operation):
        return 0
            
    return len(operation["arguments"])
# -----------------------------------------------------------
# ExecuteOp
# -----------------------------------------------------------
    
def PrintExecuteOp(file_id, device, operation):
    file_id.write('template<>\n')
    file_id.write('template<>\n')
    
    file_id.write('int KDevice<' + device.class_name + ',' \
                                    + device.name + '>::\n')
    file_id.write('        execute_op<' + device.class_name + '::' \
                            + operation["name"] + '> \n' )
                    
    file_id.write('        (const Argument<' + device.class_name + '::' \
                            + operation["name"] + '>& args, SessID sess_id)\n')
    
    file_id.write('{\n')
    
    # Load code fragments
    for frag in device.frag_handler.fragments:
        if operation["name"] == frag['name']:        
            for line in frag['fragment']:
                file_id.write(line)
    
    file_id.write('}\n\n')
    
def PrintIsFailed(file_id, device):
    file_id.write('template<>\n')
    file_id.write('bool KDevice<' + device.class_name + ',' \
                    + device.name + '>::is_failed(void)\n')
    file_id.write('{\n')
    
    for frag in device.frag_handler.fragments:
        if frag['name'] == "IS_FAILED":        
            for line in frag['fragment']:
                file_id.write(line)
    
    file_id.write('}\n\n')
    
def PrintExecute(file_id, device):
    file_id.write('template<>\n')
    file_id.write('int KDevice<' + device.class_name \
                                + ',' + device.name + '>::\n')
    file_id.write('        execute(const Command& cmd)\n' )
    file_id.write('{\n')
    
    file_id.write('#if KSERVER_HAS_THREADS\n')
    file_id.write('    std::lock_guard<std::mutex> lock(THIS->mutex);\n')
    file_id.write('#endif\n\n')
    
    file_id.write('    switch(cmd.operation) {\n')
    
    for operation in device.operations:
        file_id.write('      case ' + device.class_name + '::' \
                                + operation["name"] + ': {\n')
        file_id.write('        Argument<' + device.class_name + '::' \
                                + operation["name"] + '> args;\n\n')
        file_id.write('        if (parse_arg<' + device.class_name + '::' \
                                + operation["name"] + '>(cmd, args) < 0)\n')
        file_id.write('            return -1;\n\n')
        file_id.write('        return execute_op<' + device.class_name + '::' \
                                + operation["name"] + '>(args, cmd.sess_id);\n')                                             
        file_id.write('      }\n')
        
    file_id.write('      case ' + device.class_name + '::' \
                                + device.name.lower() + '_op_num:\n')
    file_id.write('      default:\n')
    file_id.write('          kserver->syslog.print(SysLog::ERROR, "' 
                            + device.class_name + ': Unknown operation\\n");\n')
    file_id.write('          return -1;\n')
    
    file_id.write('    }\n')
    
    file_id.write('}\n\n')

