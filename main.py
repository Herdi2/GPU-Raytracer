# User settings
nvprof = False
raytracer = True
# END
import os
path_to_exe = ".\\Pathtracer.exe "
nvprof_init = "nvprof --unified-memory-profiling off "

final_string = ""

if(nvprof):
    nvprof_in = open("nvprof.in", "r").read().split("\n")
    nvprof_metrics = ""
    if(nvprof_in[0] != ''):
        nvprof_metrics = " ".join(["--" + e for e in nvprof_in]) + " "
    final_string += nvprof_init
    final_string += nvprof_metrics

final_string += path_to_exe

if(raytracer):
    ray_tracer_in = open("pathtracer.in", "r").read().split("\n")
    ray_tracer_settings = ""
    if(ray_tracer_in[0] != ''):
        ray_tracer_settings = " ".join(["--" + e for e in ray_tracer_in]) + " "
    final_string += ray_tracer_settings

print(final_string)

os.system(final_string)

print("Test done")

