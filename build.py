import os

html = str(open("data/index.html").read())

with open("include/html.h", "w") as f:
    f.write(f'const char PROGMEM index_html[] = R"indexhtml({html})indexhtml";')

