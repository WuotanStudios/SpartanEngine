#Copyright(c) 2016-2025 Panos Karabelas
#
#Permission is hereby granted, free of charge, to any person obtaining a copy
#of this software and associated documentation files (the "Software"), to deal
#in the Software without restriction, including without limitation the rights
#to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
#copies of the Software, and to permit persons to whom the Software is furnished
#to do so, subject to the following conditions :
#
#The above copyright notice and this permission notice shall be included in
#all copies or substantial portions of the Software.
#
#THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
#FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR
#COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
#IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
#CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

import os
import shutil
import sys
import stat
import subprocess
from pathlib import Path
import file_utilities

paths = {
    "binaries": {
        "data": Path("binaries/data"),
        "models": Path("binaries/project/models"),
        "music": Path("binaries/project/music"),
        "terrain": Path("binaries/project/terrain"),
        "materials": Path("binaries/project/materials"),
    },
    "third_party_libs": {
        "dx": Path("third_party/libraries/dxcompiler.dll"),
        "fmod": Path("third_party/libraries/fmod.dll"),
        "fmod_debug": Path("third_party/libraries/fmodL.dll"),
    },
    "assets": {
        "models": Path("assets/models"),
        "music": Path("assets/music"),
        "terrain": Path("assets/terrain"),
        "materials": Path("assets/materials"),
    },
}

def is_directory(path):
    if not os.path.exists(path):
        return os.path.splitext(path)[1] == ""
    return os.path.isdir(path)

def copy(source, destination):
    def on_rm_error(func, path, exc_info):
        # make the file writable if it's read-only
        os.chmod(path, stat.S_IWRITE)
        func(path)

    if os.path.isfile(source):
        if is_directory(destination):
            dest_file = os.path.join(destination, os.path.basename(source))
        else:
            dest_file = destination
        print(f"Copying file \"{source}\" to \"{dest_file}\"...")
        if os.path.exists(dest_file):
            os.chmod(dest_file, stat.S_IWRITE)  # make the file writable if it exists
        shutil.copy2(source, dest_file)
    elif is_directory(source) and is_directory(destination):
        print(f"Copying directory \"{source}\" to directory \"{destination}\"...")
        if os.path.exists(destination):
            shutil.rmtree(destination, onerror=on_rm_error)
        shutil.copytree(source, destination, dirs_exist_ok=True)
    else:
        print(f"Error: {source} and {destination} are not compatible.")
        return False
    return True
    
def generate_project_files():
    cmd = (
        f"build_scripts\\premake5.exe --file=build_scripts\\premake.lua {sys.argv[1]} {sys.argv[2]}"
        if sys.argv[1] == "vs2022"
        else f"premake5 --file=build_scripts/premake.lua {sys.argv[1]} {sys.argv[2]}"
    )
    subprocess.Popen(cmd, shell=True).communicate()
    
    if sys.argv[1] == "vs2022" and not os.path.exists("spartan.sln"):
        print("Error: spartan.sln not generated.")
        sys.exit(1)
    elif sys.argv[1] != "vs2022" and not os.path.exists("Makefile") and not os.path.exists("editor/Makefile") and not os.path.exists("runtime/Makefile"):
        print("Error: makefiles not generated")
        sys.exit(1)

def main():
    is_ci = "ci" in sys.argv
    
    print("\n1. Create binaries folder with the required data files...\n")
    copy("data", paths["binaries"]["data"])
    copy("build_scripts/download_assets.py", "binaries/")
    copy("build_scripts/file_utilities.py", "binaries/")
    copy("build_scripts/7z.exe", "binaries/")
    copy("build_scripts/7z.dll", "binaries/")
    
    print("\n2. Download and extract libraries...")
    library_url           = 'https://www.dropbox.com/scl/fi/zq64yfpbly1goahmanm4r/libraries.7z?rlkey=m90lngvaosc9i3w8k16f1e1r6&st=5jm4fmqv&dl=1'
    library_destination   = 'third_party/libraries/libraries.7z'
    library_expected_hash = '8a20305ee9658dfdfba2aea88f26e6ee3d1330d7e6d26f42bc07bb76150ff1c5'
    file_utilities.download_file(library_url, library_destination, library_expected_hash)
    file_utilities.extract_archive("third_party/libraries/libraries.7z", "third_party/libraries/", sys.argv[1] == "vs2022", False)
    
    print("3. Copying required DLLs to the binary directory...")
    for lib in paths["third_party_libs"].values():
        copy(lib, Path("binaries"))

    print("\n4. Generate project files...\n")
    generate_project_files()
    
    if not is_ci:
        input("\nPress any key to continue...")
        
    sys.exit(0)

if __name__ == "__main__":
    main()