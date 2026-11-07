import os

def bundle_project():
    output_file = "linstack_context.txt"
    with open(output_file, "w", encoding="utf-8") as outfile:
        for root, dirs, files in os.walk("."):
            for file in files:
                    file_path = os.path.join(root, file)
                    outfile.write(f"\n--- FILE: {file_path} ---\n")
                    with open(file_path, "r", encoding="utf-8", errors="ignore") as infile:
                        outfile.write(infile.read())
    print(f"تم جمع سياق المشروع بالكامل في ملف: {output_file}")

if __name__ == "__main__":
    bundle_project()
