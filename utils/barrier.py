# Creates the barrier image needed for the game
# Creates a file called barrier.c in src/ 

import numpy as np
import os 


width, height = 220, 20 
data = np.zeros((width, height), dtype=np.uint16)

width, height = data.shape

black = 0x0000

name = "barrier"

cwd = os.getcwd()
out_file = os.path.join(cwd, "..", "src", f"{name}.c")

# write the data to a file
with open(out_file, "w") as f:
    f.write("const struct\n")
    f.write("{\n")
    f.write("unsigned int width;\n")
    f.write("unsigned int height;\n")
    f.write("unsigned int bytes_per_pixel;\n")
    f.write(f"unsigned char pixel_data[{width} * {height} * 2 + 1];\n")
    f.write("} barrier = {\n")
    f.write(f"{width},\n{height},\n2,\n")

    for y in range(height):
        f.write('    "')
        for x in range(width):
            f.write('\\%03o\\%03o' % (black&0xff, black>>8))
        f.write('"\n')

    f.write("};\n")
    
