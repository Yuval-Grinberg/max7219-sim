Import("env")  # noqa: F821  (provided by PlatformIO)
 
import glob
import inspect
import os
import re
import shutil
import subprocess
import sys
 
SCRIPT = os.path.abspath(inspect.getframeinfo(inspect.currentframe()).filename)
SIM = os.path.dirname(os.path.dirname(SCRIPT))
WINDOWS = sys.platform.startswith("win")
 
 
def log(msg):
    print("[max7219-sim] " + msg)
 
 
# ---- simulator options: platformio.ini custom_sim_args + "// sim:" comments in src/ ----
args = []
src_dir = env.subst("$PROJECT_SRC_DIR")
for path in sorted(glob.glob(os.path.join(src_dir, "**", "*.*"), recursive=True)):
    if os.path.splitext(path)[1].lower() not in (".ino", ".pde", ".cpp", ".c", ".h", ".hpp"):
        continue
    try:
        with open(path, encoding="utf-8", errors="ignore") as f:
            for line in f:
                m = re.match(r"^\s*//\s*sim:(.*)$", line)
                if m:
                    args.append(m.group(1).strip())
    except OSError:
        pass
ini_args = env.GetProjectOption("custom_sim_args", "")
if ini_args:
    args.append(" ".join(ini_args.split()))
sim_args = " ".join(a for a in args if a)
 
 
def c_literal(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'
 
 
# Written to a header so spaces and quotes don't have to survive the compiler command line.
gen_dir = os.path.join(env.subst("$BUILD_DIR"), "max7219-sim-gen")
os.makedirs(gen_dir, exist_ok=True)
header = "#pragma once\n#define SIM_ARGS %s\n#define SIM_NAME %s\n" % (
    c_literal(sim_args), c_literal(os.path.basename(env.subst("$PROJECT_DIR"))))
header_path = os.path.join(gen_dir, "sim_args.h")
if not os.path.exists(header_path) or open(header_path).read() != header:
    with open(header_path, "w") as f:
        f.write(header)
if sim_args:
    log("simulator options: " + sim_args)
 
env.Append(
    CPPPATH=[os.path.join(SIM, "core"), gen_dir],
    CPPDEFINES=["SIM_PLATFORMIO", "_USE_MATH_DEFINES"],
    CXXFLAGS=["-std=gnu++17", "-fpermissive", "-Wno-overloaded-virtual", "-Wno-sign-compare",
              "-Wno-narrowing", "-Wno-unused-variable"],
    CFLAGS=["-Wno-unused-variable"],
    # Adafruit_GFX and Max72xxPanel bundled with the simulator
    LIBSOURCE_DIRS=[os.path.join(SIM, "libraries")],
)
 
# ---- SDL2 for the window ----
def split_flags(text):
    """Split pkg-config output into flags. pkg-config writes a space inside a path as
    "\\ " (e.g. D:/Program\\ Files/msys64); any other backslash is a Windows path separator."""
    out, cur, i = [], "", 0
    quote = None
    while i < len(text):
        c = text[i]
        if quote:
            if c == quote:
                quote = None
            else:
                cur += c
        elif c in "\"'":
            quote = c
        elif c == "\\" and i + 1 < len(text) and text[i + 1] in " \t":
            cur += text[i + 1]
            i += 1
        elif c.isspace():
            if cur:
                out.append(cur)
            cur = ""
        else:
            cur += c
        i += 1
    if cur:
        out.append(cur)
    return out
 
 
def pkg_config(*a):
    exe = shutil.which("pkg-config") or shutil.which("pkgconf")
    if not exe:
        return None
    try:
        return split_flags(subprocess.check_output([exe] + list(a) + ["sdl2"], stderr=subprocess.DEVNULL, text=True))
    except (OSError, subprocess.CalledProcessError):
        return None
 
 
def clean_path(p):
    return os.path.normpath(p).replace("\\", "/")
 
 
def apply_flags(flags, compile_flags):
    """Add pkg-config style flags to the build without SCons' shell-style parser,
    which treats the backslashes in Windows paths as escape characters."""
    for f in flags:
        f = f.strip().strip('"')
        if f in ("-mwindows", "-lSDL2main", "-Dmain=SDL_main", "-Dmain"):
            continue  # SDL's main() wrapper / GUI subsystem would hide the Serial console
        if f.startswith("-I") and len(f) > 2:
            env.Append(CPPPATH=[clean_path(f[2:])])
        elif f.startswith("-L") and len(f) > 2:
            env.Append(LIBPATH=[clean_path(f[2:])])
        elif f.startswith("-l") and len(f) > 2:
            env.Append(LIBS=[f[2:]])
        elif f.startswith("-D") and len(f) > 2:
            name, _, value = f[2:].partition("=")
            env.Append(CPPDEFINES=[(name, value)] if value else [name])
        elif f:
            env.Append(CCFLAGS=[f]) if compile_flags else env.Append(LINKFLAGS=[f])
 
 
cflags = pkg_config("--cflags")
libs = pkg_config("--static", "--libs") if WINDOWS else pkg_config("--libs")
if WINDOWS and not (cflags and any("SDL2" in f for f in cflags)):
    # pkg-config missing, or a different one (e.g. from Strawberry Perl): use MSYS2's SDL2 directly
    cflags = libs = None
    roots = []
    gcc = shutil.which("gcc")
    if gcc:  # e.g. D:\\Program Files\\msys64\\ucrt64\\bin\\gcc.exe -> ...\\ucrt64
        roots.append(os.path.dirname(os.path.dirname(gcc)))
    roots += ["C:/msys64/ucrt64", "C:/msys64/mingw64"]
    for root in roots:
        root = clean_path(root)
        if os.path.exists(root + "/include/SDL2/SDL.h"):
            cflags = ["-I" + root + "/include/SDL2"]
            libs = ["-L" + root + "/lib", "-lSDL2", "-lsetupapi", "-lwinmm", "-limm32", "-lversion",
                    "-lole32", "-loleaut32", "-lgdi32", "-luser32", "-lshell32", "-ladvapi32", "-lcfgmgr32",
                    "-luuid", "-ldinput8"]
            break
 
if cflags is not None and libs is not None:
    env.Append(CPPDEFINES=["SIM_HAVE_SDL"])
    apply_flags(cflags, True)
    apply_flags(libs, False)
    log("SDL2 include: " + ", ".join(clean_path(f[2:]) for f in cflags if f.startswith("-I")))
else:
    log("SDL2 not found - the matrix will be drawn in the terminal (install SDL2 for a window).")
 
if WINDOWS:
    env.Append(LINKFLAGS=["-static"])  # no DLLs needed next to program.exe
else:
    env.Append(LIBS=["pthread"])
 
# ---- the simulator itself ----
env.BuildSources(
    os.path.join("$BUILD_DIR", "max7219-sim"),
    SIM,
    src_filter=["-<*>", "+<core/*.cpp>", "+<sim/*.cpp>"],
)
 
# ---- "Simulate" button in the PlatformIO sidebar (sim > Custom > Simulate) ----
env.AddCustomTarget(
    name="simulate",
    dependencies="$PROG_PATH",
    actions=['"$PROG_PATH"'],
    title="Simulate",
    description="Build and open the MAX7219 simulator",
)
 