// ============================================================
//  学生成绩管理系统 v2
//  在 v1 基础上新增：
//    - 数据持久化：程序退出数据不丢，下次打开还在（students.txt）
//    - 修改成绩
//    - 删除学生
//    - 班级统计（及格率、最高分、最低分）
//  编译：双击 build.bat
//  运行：双击 run.bat
// ============================================================

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <iomanip>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

// ---------- 1. 全局配置 ----------
const int SUBJECT_COUNT = 3;
const string SUBJECT_NAMES[SUBJECT_COUNT] = {"数学", "英语", "C++"};
const string DATA_FILE = "students.txt";     // 数据存档文件名

// ---------- 2. 数据模型 ----------
struct Student {
    string id;
    string name;
    double scores[SUBJECT_COUNT];
};

// ---------- 3. 计算函数 ----------
double total(const Student& s) {
    double sum = 0;
    for (int i = 0; i < SUBJECT_COUNT; i++) {
        sum += s.scores[i];
    }
    return sum;
}

double average(const Student& s) {
    return total(s) / SUBJECT_COUNT;
}

// ---------- 4. 输入保护 ----------
// 用户在"输入数字"的地方打了字母，cin 会进入错误状态，
// 之后所有输入都会失效、程序会疯狂死循环。
// 所以统一用这两个函数兜底：读失败就清掉错误状态、丢弃这一行，返回 false。
bool readInt(int& out) {
    if (cin >> out) return true;
    cin.clear();                        // 清除错误标记
    cin.ignore(10000, '\n');            // 把这一行剩下的垃圾字符丢掉
    return false;
}

bool readDouble(double& out) {
    if (cin >> out) return true;
    cin.clear();
    cin.ignore(10000, '\n');
    return false;
}

// ---------- 5. 显示函数 ----------
// 不用"表格对齐"：中文字符显示宽度是英文的两倍，
// 用 setw 按字符数补空格永远对不齐。改成一行一个学生的标签式输出。
void printOne(const Student& s, int rank) {
    if (rank > 0) {
        cout << "  " << rank << ". ";
    } else {
        cout << "     ";
    }

    cout << s.name << "（" << s.id << "）   ";

    for (int i = 0; i < SUBJECT_COUNT; i++) {
        cout << SUBJECT_NAMES[i] << " "
             << fixed << setprecision(0) << s.scores[i] << "   ";
    }

    cout << "|  总分 " << setprecision(0) << total(s)
         << "   平均分 " << setprecision(1) << average(s) << endl;
}

void printAll(const vector<Student>& list) {
    if (list.empty()) {
        cout << "\n[提示] 目前还没有学生，请先选 1 添加。\n";
        return;
    }
    cout << "\n===== 全部学生 =====\n";
    for (int i = 0; i < (int)list.size(); i++) {
        printOne(list[i], 0);
    }
    cout << "共 " << list.size() << " 人\n";
}

void printRanking(const vector<Student>& list) {
    if (list.empty()) {
        cout << "\n[提示] 目前还没有学生。\n";
        return;
    }

    // 先复制一份再排序，不动原数据
    vector<Student> sorted = list;
    sort(sorted.begin(), sorted.end(), [](const Student& a, const Student& b) {
        return average(a) > average(b);     // > 表示从高到低
    });

    cout << "\n===== 成绩排名 =====\n";
    for (int i = 0; i < (int)sorted.size(); i++) {
        printOne(sorted[i], i + 1);
    }
}

// ---------- 5.1 选科目（4、5 两个功能共用）----------
// 让用户选一门课，返回它的下标 0..SUBJECT_COUNT-1；用户放弃则返回 -1
int chooseSubject() {
    cout << "\n选择科目：\n";
    for (int i = 0; i < SUBJECT_COUNT; i++) {
        cout << "  " << (i + 1) << ". " << SUBJECT_NAMES[i] << "\n";
    }
    cout << "科目编号: ";

    int n = 0;
    if (!readInt(n)) return -1;
    if (n < 1 || n > SUBJECT_COUNT) return -1;
    return n - 1;
}

// ---------- 5.2 单科排名 ----------
void printSubjectRanking(const vector<Student>& list) {
    if (list.empty()) {
        cout << "\n[提示] 目前还没有学生。\n";
        return;
    }

    int sub = chooseSubject();
    if (sub == -1) {
        cout << "[错误] 科目编号无效。\n";
        return;
    }

    // 和总排名一样的套路，只是比较的字段换了
    vector<Student> sorted = list;
    sort(sorted.begin(), sorted.end(), [sub](const Student& a, const Student& b) {
        return a.scores[sub] > b.scores[sub];      // [sub] 把科目带进 lambda
    });

    cout << "\n===== " << SUBJECT_NAMES[sub] << " 单科排名 =====\n";
    for (int i = 0; i < (int)sorted.size(); i++) {
        printOne(sorted[i], i + 1);
    }
}

// ---------- 5.3 按分数筛选 ----------
void filterByScore(const vector<Student>& list) {
    if (list.empty()) {
        cout << "\n[提示] 目前还没有学生。\n";
        return;
    }

    int sub = chooseSubject();
    if (sub == -1) {
        cout << "[错误] 科目编号无效。\n";
        return;
    }

    cout << "\n筛选方式：\n";
    cout << "  1. 高于或等于某个分数\n";
    cout << "  2. 低于某个分数\n";
    cout << "选择: ";
    int mode = 0;
    if (!readInt(mode) || (mode != 1 && mode != 2)) {
        cout << "[错误] 只能输入 1 或 2。\n";
        return;
    }

    cout << "分数: ";
    double line = 0;
    if (!readDouble(line)) {
        cout << "[错误] 请输入数字。\n";
        return;
    }

    int count = 0;
    cout << "\n===== " << SUBJECT_NAMES[sub];
    // 这里必须加 fixed，否则 80 会被输出成科学计数法 8e+01
    if (mode == 1) cout << " >= " << fixed << setprecision(0) << line;
    else           cout << " < "  << fixed << setprecision(0) << line;
    cout << " 的学生 =====\n";

    for (int i = 0; i < (int)list.size(); i++) {
        bool hit = (mode == 1) ? (list[i].scores[sub] >= line)
                               : (list[i].scores[sub] <  line);
        if (hit) {
            count++;
            printOne(list[i], count);
        }
    }

    if (count == 0) cout << "  没有符合条件的学生。\n";
    else            cout << "共 " << count << " 人\n";
}

// ---------- 6. 班级统计 ----------
void printStats(const vector<Student>& list) {
    if (list.empty()) {
        cout << "\n[提示] 目前还没有学生。\n";
        return;
    }

    double sumAvg = 0;
    double maxAvg = -1, minAvg = 1000;
    string maxName, minName;
    int passCount = 0;

    for (int i = 0; i < (int)list.size(); i++) {
        double avg = average(list[i]);
        sumAvg += avg;

        if (avg > maxAvg) { maxAvg = avg; maxName = list[i].name; }
        if (avg < minAvg) { minAvg = avg; minName = list[i].name; }

        // 及格 = 所有科目都不低于 60 分
        bool pass = true;
        for (int j = 0; j < SUBJECT_COUNT; j++) {
            if (list[i].scores[j] < 60) { pass = false; break; }
        }
        if (pass) passCount++;
    }

    cout << "\n===== 班级统计 =====\n";
    cout << "  人数        : " << list.size() << " 人\n";
    cout << "  班级平均分  : " << fixed << setprecision(1) << (sumAvg / list.size()) << endl;
    cout << "  最高平均分  : " << setprecision(1) << maxAvg << "（" << maxName << "）\n";
    cout << "  最低平均分  : " << setprecision(1) << minAvg << "（" << minName << "）\n";
    cout << "  全科及格人数: " << passCount << " / " << list.size()
         << "（及格率 " << setprecision(1)
         << (passCount * 100.0 / list.size()) << "%）\n";

    // 每门课的平均分
    cout << "  各科平均分  :";
    for (int j = 0; j < SUBJECT_COUNT; j++) {
        double sum = 0;
        for (int i = 0; i < (int)list.size(); i++) sum += list[i].scores[j];
        cout << "  " << SUBJECT_NAMES[j] << " " << setprecision(1) << (sum / list.size());
    }
    cout << endl;

    // ---- 分数区间分布（按平均分划分）----
    // 数组的每个格子对应一个档位的计数：
    //   [0] 优秀 >=90   [1] 良好 80-89   [2] 中等 70-79
    //   [3] 及格 60-69  [4] 不及格 <60
    // 注意：标签故意不用 setw 补空格（中文字符宽度不一样，对不齐），
    // 而是靠"前四档补两个空格"来手工对齐，因为"不及格"比它们多一个汉字。
    string labels[5] = {"优秀", "良好", "中等", "及格", "不及格"};
    string ranges[5] = {">=90", "80-89", "70-79", "60-69", "<60"};
    int buckets[5] = {0, 0, 0, 0, 0};

    for (int i = 0; i < (int)list.size(); i++) {
        double avg = average(list[i]);
        if      (avg >= 90) buckets[0]++;
        else if (avg >= 80) buckets[1]++;
        else if (avg >= 70) buckets[2]++;
        else if (avg >= 60) buckets[3]++;
        else                buckets[4]++;
    }

    cout << "\n  平均分分布：\n";
    for (int k = 0; k < 5; k++) {
        cout << "    " << labels[k];
        if (k < 4) cout << "  ";        // 前四档标签少一个字，补两个空格对齐
        cout << "  " << ranges[k] << "   " << buckets[k] << " 人   ";
        for (int c = 0; c < buckets[k]; c++) cout << "█";   // 简易条形图
        cout << "\n";
    }
}

// ---------- 7. 文件读写（数据持久化）----------
// 保存格式：一行一个学生，空格分隔
//   2026001 张三 88 79 92
void saveToFile(const vector<Student>& list) {
    ofstream fout(DATA_FILE);
    if (!fout) {
        cout << "[警告] 无法写入 " << DATA_FILE << "，数据只在本次运行有效。\n";
        return;
    }
    for (int i = 0; i < (int)list.size(); i++) {
        fout << list[i].id << " " << list[i].name;
        for (int j = 0; j < SUBJECT_COUNT; j++) {
            fout << " " << list[i].scores[j];
        }
        fout << "\n";
    }
    fout.close();
}

void loadFromFile(vector<Student>& list) {
    ifstream fin(DATA_FILE);
    if (!fin) return;                   // 第一次运行没有存档，正常

    Student s;
    while (fin >> s.id >> s.name) {     // 读不到就结束循环
        bool ok = true;
        for (int j = 0; j < SUBJECT_COUNT; j++) {
            if (!(fin >> s.scores[j])) { ok = false; break; }
        }
        if (ok) list.push_back(s);
    }
    fin.close();

    if (!list.empty()) {
        cout << "[提示] 已从 " << DATA_FILE << " 读取 " << list.size() << " 条记录。\n";
    }
}

// ---------- 8. 查找 ----------
// 返回学号在列表里的下标，找不到返回 -1
int findById(const vector<Student>& list, const string& id) {
    for (int i = 0; i < (int)list.size(); i++) {
        if (list[i].id == id) return i;
    }
    return -1;
}

// ---------- 9. 添加 ----------
void addStudent(vector<Student>& list) {
    Student s;
    cout << "\n--- 添加学生 ---\n";
    cout << "学号: ";
    cin >> s.id;

    if (findById(list, s.id) != -1) {
        cout << "[错误] 学号 " << s.id << " 已存在，添加取消。\n";
        return;
    }

    cout << "姓名: ";
    cin >> s.name;

    for (int i = 0; i < SUBJECT_COUNT; i++) {
        while (true) {
            cout << SUBJECT_NAMES[i] << "成绩: ";
            if (readDouble(s.scores[i])) break;
            cout << "[错误] 请输入数字。\n";
        }
    }

    list.push_back(s);
    saveToFile(list);                   // 改完立刻存档
    cout << "[成功] 已添加 " << s.name << "（自动保存）\n";
}

// ---------- 10. 修改（姓名 + 成绩）----------
void editStudent(vector<Student>& list) {
    if (list.empty()) {
        cout << "\n[提示] 没有学生可修改。\n";
        return;
    }

    cout << "\n--- 修改学生信息 ---\n";
    cout << "要修改的学号: ";
    string id;
    cin >> id;

    int idx = findById(list, id);
    if (idx == -1) {
        cout << "[错误] 找不到学号 " << id << "\n";
        return;
    }

    cout << "找到了：" << list[idx].name << "（" << list[idx].id << "），当前信息：\n";
    printOne(list[idx], 0);

    // --- 改姓名 ---
    // 学号不开放修改：它既是查找的钥匙，也是网页找照片的文件名。
    // 真要改学号，请删除后重新添加（顺便避免出现两个同号记录）。
    cout << "\n新姓名（输入 - 表示不改）: ";
    string newName;
    cin >> newName;
    if (newName != "-") {
        cout << "姓名：" << list[idx].name << " -> " << newName << "\n";
        list[idx].name = newName;
    }

    // --- 改成绩 ---
    for (int i = 0; i < SUBJECT_COUNT; i++) {
        cout << "新" << SUBJECT_NAMES[i] << "成绩（输入 -1 表示不改）: ";
        double v;
        while (true) {
            if (readDouble(v)) break;
            cout << "[错误] 请输入数字。\n";
        }
        if (v >= 0) list[idx].scores[i] = v;
    }

    saveToFile(list);
    cout << "[成功] 已修改并保存。\n";
}

// ---------- 11. 删除 ----------
void deleteStudent(vector<Student>& list) {
    if (list.empty()) {
        cout << "\n[提示] 没有学生可删除。\n";
        return;
    }

    cout << "\n--- 删除学生 ---\n";
    cout << "要删除的学号: ";
    string id;
    cin >> id;

    int idx = findById(list, id);
    if (idx == -1) {
        cout << "[错误] 找不到学号 " << id << "\n";
        return;
    }

    cout << "确认删除 " << list[idx].name << "（" << list[idx].id << "）吗？(y/n): ";
    string answer;
    cin >> answer;
    if (answer != "y" && answer != "Y") {
        cout << "已取消。\n";
        return;
    }

    // erase 会把指定位置删掉，后面的元素自动前移
    list.erase(list.begin() + idx);
    saveToFile(list);
    cout << "[成功] 已删除并保存。剩余 " << list.size() << " 人。\n";
}

// ---------- 12. 导出给网页 ----------
void exportData(const vector<Student>& list) {
    // a) CSV，给 Excel 看
    ofstream csv("grades.csv");
    csv << "学号,姓名";
    for (int i = 0; i < SUBJECT_COUNT; i++) csv << "," << SUBJECT_NAMES[i];
    csv << ",总分,平均分\n";
    for (int i = 0; i < (int)list.size(); i++) {
        const Student& s = list[i];
        csv << s.id << "," << s.name;
        for (int j = 0; j < SUBJECT_COUNT; j++) csv << "," << s.scores[j];
        csv << "," << total(s) << "," << average(s) << "\n";
    }
    csv.close();

    // b) data.js，给网页看
    // 为什么不是 json？浏览器用 file:// 打开网页时读 json 会被 CORS 拦，
    // 而 <script src="data.js"> 不受限制，这是不用起服务器的最简方案。
    ofstream js("web/data.js");
    js << "// 本文件由 C++ 程序自动生成，请勿手改\n";
    js << "var STUDENTS = [\n";
    for (int i = 0; i < (int)list.size(); i++) {
        const Student& s = list[i];
        js << "  { id: \"" << s.id << "\", name: \"" << s.name << "\", scores: [";
        for (int j = 0; j < SUBJECT_COUNT; j++) {
            js << s.scores[j];
            if (j < SUBJECT_COUNT - 1) js << ", ";
        }
        js << "] }";
        if (i < (int)list.size() - 1) js << ",";
        js << "\n";
    }
    js << "];\n";
    js << "var SUBJECTS = [";
    for (int i = 0; i < SUBJECT_COUNT; i++) {
        js << "\"" << SUBJECT_NAMES[i] << "\"";
        if (i < SUBJECT_COUNT - 1) js << ", ";
    }
    js << "];\n";
    js.close();

    cout << "\n[完成] 已导出 " << list.size() << " 条数据\n";
    cout << "       grades.csv   （可用 Excel 打开）\n";
    cout << "       web/data.js  （网页会自动读取）\n";
    cout << "       现在打开 web/index.html 就能看到图表\n";
}

// ---------- 13. 主函数 ----------
int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    vector<Student> list;
    loadFromFile(list);                 // 启动时先读存档

    int choice = -1;

    while (true) {
        cout << "\n========== 学生成绩管理系统 ==========\n";
        cout << "  1. 添加学生\n";
        cout << "  2. 查看全部\n";
        cout << "  3. 总排名（按平均分）\n";
        cout << "  4. 单科排名\n";
        cout << "  5. 按分数筛选\n";
        cout << "  6. 修改成绩\n";
        cout << "  7. 删除学生\n";
        cout << "  8. 班级统计\n";
        cout << "  9. 导出到网页\n";
        cout << "  0. 退出\n";
        cout << "======================================\n";
        cout << "请选择: ";

        if (!readInt(choice)) {
            cout << "[错误] 请输入数字。\n";
            continue;
        }

        if (choice == 1) {
            addStudent(list);
        } else if (choice == 2) {
            printAll(list);
        } else if (choice == 3) {
            printRanking(list);
        } else if (choice == 4) {
            printSubjectRanking(list);
        } else if (choice == 5) {
            filterByScore(list);
        } else if (choice == 6) {
            editStudent(list);
        } else if (choice == 7) {
            deleteStudent(list);
        } else if (choice == 8) {
            printStats(list);
        } else if (choice == 9) {
            if (list.empty()) {
                cout << "\n[提示] 没有数据可导出。\n";
            } else {
                exportData(list);
            }
        } else if (choice == 0) {
            saveToFile(list);
            cout << "\n已保存 " << list.size() << " 条记录到 " << DATA_FILE << "，退出。\n";
            break;
        } else {
            cout << "\n[错误] 只能输入 0-9。\n";
        }
    }

    return 0;
}
