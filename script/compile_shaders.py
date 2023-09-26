import os
import sys

path_env = os.environ.get('PATH')
path_dirs = path_env.split(os.pathsep)
executable_name = "glslc.exe"
src_path = os.path.dirname(os.path.abspath(__file__)) + "\\..\\sources"
dst_path = os.path.dirname(os.path.abspath(__file__)) + "\\..\\resources\\shaders"

# 获取glsc.exe
def Find_glslc():
    for directory in path_dirs:
        executable_path = os.path.join(directory, executable_name)
        if os.path.isfile(executable_path) and os.access(executable_path, os.X_OK):
            return executable_path
    else:
        sys.exit("Could not find glslc in PATH")

glslc = Find_glslc()

# 清空原来的resources/shaders文件夹
if os.path.exists(dst_path):
    for item in os.listdir(dst_path):
        item_path = os.path.join(dst_path, item)
        os.remove(item_path)
else:
    os.mkdir(dst_path)

# 所有shaders文件夹
def list_subdirectories(root_dir):
    subdirectories = []
    for item in os.listdir(root_dir):
        if(item == "shaders"):
            subdirectories.append(root_dir + "\\shaders")
        item_path = os.path.join(root_dir, item)
        if os.path.isdir(item_path):
            subdirectories.extend(list_subdirectories(item_path))
    return subdirectories

# 将所有shaders文件夹中的shader文件编译到 resources/shaders文件夹中
for subdir in list_subdirectories(src_path):
    parts = subdir.split("\\")
    pre = parts[-3].split("_")[0] + "_" + parts[-2].split("_")[0] + "_"
    for shader in os.listdir(subdir):
        shader_path = os.path.join(subdir, shader)
        spv_path = os.path.join(dst_path, pre + shader.replace(".", "_") + ".spv")
        # print("Shader", shader_path)
        # print("SPV", spv_path)
        os.system(glslc + " " + shader_path + " -o " + spv_path)

print("Shaders compilation completed")