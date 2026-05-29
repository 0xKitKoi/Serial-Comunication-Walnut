import sys

with open("Helvetica.ttf", "rb") as f:
    data = f.read()

with open("Helvetica.embed", "w") as f:
    f.write("static const unsigned char g_HelveticaFont[] = {\n    ")
    for i, byte in enumerate(data):
        f.write(f"0x{byte:02X}, ")
        if (i + 1) % 16 == 0:
            f.write("\n    ")
    f.write("\n};\n")