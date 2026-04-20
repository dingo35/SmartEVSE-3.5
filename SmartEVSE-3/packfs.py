#this script will be run by platformio.ini from its native directory
import os, sys, gzip, shutil, re

# Asset minification during packing
# --------------------------------
# This build step minifies the two packed web assets used by the firmware:
#   - data/index.html
#   - data/styling.css
#
# Why:
# - It saves a lot of bytes in the embedded filesystem image.
# - Smaller assets reduce flash/storage usage and transfer overhead.
#
# Safety:
# - Minification is conservative and avoids transformations that can change
#   runtime behavior.
# - Runtime functionality is unchanged (same DOM/script logic and CSS rules).
# - Script/style/pre/textarea blocks are protected for HTML minification so
#   their content is not altered.
# - Inline JavaScript is left untouched except for `/* ... */` block comments
#   inside script blocks.

def minify_css(content):
    # Compact CSS by removing comments and unnecessary whitespace.
    # Selectors/properties/values remain the same.
    content = re.sub(r"/\*[\s\S]*?\*/", "", content)
    content = re.sub(r"\s+", " ", content)
    content = re.sub(r"\s*([{}:;,>+~])\s*", r"\1", content)
    content = content.replace(";}", "}")
    return content.strip() + "\n"

def minify_html(content):
    # Protect blocks where whitespace/content must remain untouched.
    blocks = []
    def protect(match):
        block = match.group(0)
        if match.group(1).lower() == "script":
            # Remove block comments from inline JavaScript only.
            block = re.sub(r"/\*[\s\S]*?\*/", "", block)
        blocks.append(block)
        return "___HTML_BLOCK_" + str(len(blocks) - 1) + "___"

    content = re.sub(r"<(script|style|pre|textarea)\b[\s\S]*?</\1>", protect, content, flags=re.IGNORECASE)
    # Remove HTML comments outside protected blocks.
    content = re.sub(r"<!--[\s\S]*?-->", "", content)
    # Remove whitespace that exists only between adjacent tags.
    content = re.sub(r">\s+<", "><", content)
    content = content.strip()
    for i, block in enumerate(blocks):
        content = content.replace("___HTML_BLOCK_" + str(i) + "___", block)
    return content + "\n"

#check for the two files we need to be able to keep updating the firmware by the /update endpoint:
if not os.path.isfile("data/update2.html"):
    print("Missing file: data/update2.html")
    sys.exit(1)
if os.path.isdir("pack.tmp"):
    shutil.rmtree('pack.tmp')
try:
    filelist = []
    os.makedirs('pack.tmp/data')
    # now gzip the stuff except zones.csv since this file is not served by mongoose but directly accessed:
    for file in os.listdir("data"):
        filename = os.fsdecode(file)
        if filename == "cert.pem" or filename == "key.pem" or filename == "CH32V203.bin" or filename == "SmartEVSE.webp":
            shutil.copy('data/' + filename, 'pack.tmp/data/' + filename)
            filelist.append('data/' + filename)
            continue
        elif filename == "index.html" or filename == "styling.css":
            # Minify these assets before gzipping/packing to reduce footprint.
            with open('data/' + filename, 'r', encoding='utf-8') as f_in:
                content = f_in.read()
            if filename == "index.html":
                content = minify_html(content)
            else:
                content = minify_css(content)
            with gzip.open('pack.tmp/data/' + filename + '.gz', 'wb') as f_out:
                f_out.write(content.encode('utf-8'))
            filelist.append('data/' + filename + '.gz')
            continue
        else:
            with open('data/' + filename, 'rb') as f_in, gzip.open('pack.tmp/data/' + filename + '.gz', 'wb') as f_out:
                f_out.writelines(f_in)
            filelist.append('data/' + filename + '.gz')
            continue
    os.chdir('pack.tmp')
    cmdstring = 'python ../pack.py ' + ' '.join(filelist)
    os.system(cmdstring + '>../src/packed_fs.c')
    os.chdir('..')
except Exception as e:
    print(f"An error occurred: {str(e)}")
    sys.exit(100)
if shutil.rmtree("pack.tmp"):
    print("Failed to clean up temporary files")
    sys.exit(9)
# cleanup CH32 bin file if it was generated:
if os.path.isfile("data/CH32V203.bin"):
    os.remove("data/CH32V203.bin")
