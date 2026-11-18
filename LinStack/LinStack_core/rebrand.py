import os

# قاموس الاستبدال الصارم (تطابق الحروف المتتالية فقط)
REPLACEMENTS = {
    "id.waydro": "id.linstack",
    "id.Waydro": "id.Linstack",
    "WayDroid": "LinStack",
    "waydroid": "linstack",
    "Waydroid": "Linstack",
    "WAYDROID": "LINSTACK"
}

def replace_raw_text(file_path):
    """استبدال أعمى ومباشر للحروف المتتالية داخل الملف"""
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()

        modified = False
        for old, new in REPLACEMENTS.items():
            if old in content:
                content = content.replace(old, new)
                modified = True

        if modified:
            with open(file_path, 'w', encoding='utf-8', errors='ignore') as f:
                f.write(content)
            print(f"[تعديل محتوى] -> {os.path.basename(file_path)}")
    except Exception as e:
        print(f"خطأ في قراءة {os.path.basename(file_path)}: {e}")

def run_rebrand_here():
    # جلب مسار المجلد الحالي الذي يتواجد فيه السكربت تلقائياً
    current_directory = os.path.dirname(os.path.abspath(__file__))
    print(f"🔄 جاري العمل التلقائي داخل المجلد: {current_directory}\n")

    for root, dirs, files in os.walk(current_directory, topdown=False):
        # تجنب تعديل ملف السكربت نفسه أثناء التشغيل
        if "rebrand.py" in files:
            files.remove("rebrand.py")

        # 1. استبدال الحروف داخل محتوى الملفات
        for file in files:
            file_path = os.path.join(root, file)
            replace_raw_text(file_path)

        # 2. تغيير أسماء الملفات بناءً على تطابق الحروف
        for file in files:
            new_file_name = file
            for old, new in REPLACEMENTS.items():
                new_file_name = new_file_name.replace(old, new)

            if new_file_name != file:
                old_path = os.path.join(root, file)
                new_path = os.path.join(root, new_file_name)
                os.rename(old_path, new_path)
                print(f"[تغيير اسم ملف]: {file} -> {new_file_name}")

        # 3. تغيير أسماء المجلدات بناءً على تطابق الحروف
        for d in dirs:
            new_dir_name = d
            for old, new in REPLACEMENTS.items():
                new_dir_name = new_dir_name.replace(old, new)

            if new_dir_name != d:
                old_path = os.path.join(root, d)
                new_path = os.path.join(root, new_dir_name)
                os.rename(old_path, new_path)
                print(f"[تغيير اسم مجلد]: {d} -> {new_dir_name}")

if __name__ == "__main__":
    run_rebrand_here()
    print("\n✨ تم تطهير المجلد الحالي وكافة محتوياته بنجاح!")
