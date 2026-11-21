import os

def bundle_project():
    output_file = "linstack_context.txt"
    script_name = os.path.basename(__file__)

    # تحديد المجلدات والملفات المراد تجاهلها
    ignored_dirs = {".git", "build", "nlohmann"}
    ignored_files = {"system.img", "vendor.img", output_file, script_name}

    with open(output_file, "w", encoding="utf-8") as outfile:
        for root, dirs, files in os.walk("."):
            # تعديل قائمة dirs في مكانها لمنع os.walk من الدخول إلى المجلدات المتجاهلة
            dirs[:] = [d for d in dirs if d not in ignored_dirs]

            for file in files:
                # تخطي الملفات المحددة
                if file in ignored_files:
                    continue

                file_path = os.path.join(root, file)
                outfile.write(f"\n--- FILE: {file_path} ---\n")
                with open(file_path, "r", encoding="utf-8", errors="ignore") as infile:
                    outfile.write(infile.read())

    print(f"تم جمع سياق المشروع بالكامل في ملف: {output_file}")

if __name__ == "__main__":
    bundle_project()
