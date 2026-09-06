#include <iostream>
#include <string>
#include <regex>
#include <fstream>
#include <unordered_set>
#include <random>
#include <chrono>
#include <thread>

#include "filesystem.hpp"
#include "wml_exception.hpp"
#include <SDL_log.h>

// ================= 定义文件信息结构体 =================
struct FileEntry {
    std::string name;       // 文件名，例如 "f16-f32-vcvt-neonfp16-u16.c"
    std::string rel_path;   // 工程根目录相对路径，例如 "external/tensorflow/.../f16-f32-vcvt-neonfp16-u16.c"
    std::string type;       // 文件类型，例如 "sourcecode.c.c" 或 "sourcecode.cpp.cpp"
};

// ================= 配置区域 (你需要根据实际情况修改) =================
const std::string PBXPROJ_PATH = "C:/ddksample/Xcode-iOS/kdesktop.xcodeproj/project.pbxproj";
const std::string GROUP_UUID = "2158729E3004A8270033F4CB"; // 目标 Group 的 UUID

// 填入你需要添加的所有文件列表
const std::vector<FileEntry> FILES_TO_ADD = {
    // {"f16-f32-vcvt-neonfp16-u16.c", "external/tensorflow/XNNPACK/src/f16-f32-vcvt/gen/f16-f32-vcvt-neonfp16-u16.c", "sourcecode.c.c"},
    // {"f16-rdmax-2p2x-scalar-c2.c", "external/tensorflow/XNNPACK/src/f16-rdminmax/gen/f16-rdmax-2p2x-scalar-c2.c", "sourcecode.c.c"},
    // {"arena_planner.cc", "external/tensorflow/tensorflow/lite/arena_planner.cc", "sourcecode.cpp.cpp"}, // .cc 要用 cpp 类型
	// {"f16-f32-vcvt/gen/f16-f32-vcvt-neonfp16-u16.c", "", "sourcecode.c.c"},
	// {"f16-rdminmax/gen/f16-rdmax-2p2x-scalar-c2.c", "", "sourcecode.c.c"},
	// {"f16-rdminmax/gen/f16-rdmin-2p2x-scalar-c2.c", "", "sourcecode.c.c"},
	{"../../../external/tensorflow/XNNPACK/src/f16-qs8-vcvt/gen/f16-qs8-vcvt-scalar-imagic-u4.c", "", "sourcecode.c.c"},
	{"../../../external/tensorflow/XNNPACK/src/f16-qu8-vcvt/gen/f16-qu8-vcvt-scalar-imagic-u4.c", "", "sourcecode.c.c"},
	{"../../../external/tensorflow/XNNPACK/src/f16-vapproxgelu/gen/f16-vapproxgelu-scalar-rational-6-4-div.c", "", "sourcecode.c.c"},
	{"../../../external/tensorflow/XNNPACK/src/f16-vgelu/gen/f16-vgelu-scalar-rational-6-4-div.c", "", "sourcecode.c.c"},
    // ... 这里可以放几千个文件
};
// =========================================================================

// 1. 负责解析 project.pbxproj 并构建已用 UUID 的黑名单
std::unordered_set<std::string> load_existing_uuids(const std::string& pbxproj_path)
{
    std::unordered_set<std::string> existing_uuids;
    std::string content = read_file(pbxproj_path);
    VALIDATE(!content.empty(), null_str);

    // 正则匹配所有 24位 的十六进制字符 (忽略大小写)
    //", "", "sourcecode.c.c"},b 代表单词边界，防止匹配到更长的数
    std::regex uuid_pattern("\\b[0-9A-Fa-f]{24}\\b");
    std::smatch match;
    std::string::const_iterator search_start(content.cbegin());
    
    while (std::regex_search(search_start, content.cend(), match, uuid_pattern)) {
        // 转成大写后插入集合
        std::string uuid = match[0];
        for (char& c : uuid) c = toupper(c);
        existing_uuids.insert(uuid);
        search_start = match.suffix().first;
    }

    // std::cout << "扫描完毕：项目中已存在 " << existing_uuids.size() << " 个 UUID。" << std::endl;
    return existing_uuids;
}

// 2. 安全生成器：防冲突 UUID 生成器
std::string generate_safe_xcode_uuid(const std::unordered_set<std::string>& used_uuids, const std::string& avoid_prefix)
{
    // 随机种子：时间戳 + 线程ID
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::seed_seq seed{ (unsigned int)now, (unsigned int)(now >> 32), (unsigned int)std::random_device{}() };
    std::mt19937 gen(seed);
    std::uniform_int_distribution<> dis(0, 15);

    std::string candidate;
    // 循环直到生成一个没有出现过的 UUID，且不包含避讳的前缀
    do {
        candidate.clear();
        // 优化：1次生成24位字符，不要一次一次的 +=
        for (int i = 0; i < 24; ++i) {
            int val = dis(gen);
            candidate += (val < 10) ? ('0' + val) : ('A' + val - 10);
        }

        // 如果设置了 avoid_prefix，且 candidate 以该前缀开头，则判定为非法，继续循环
        if (!avoid_prefix.empty() && candidate.find(avoid_prefix) == 0) {
            continue; 
        }

        // 检查是否在已有的 UUID 黑名单中
    } while (used_uuids.find(candidate) != used_uuids.end());

    return candidate;
}

int edit_project_pbxproj()
{
    // 1. 读取内容并建立黑名单
    std::string content = read_file(PBXPROJ_PATH);
    std::unordered_set<std::string> used_uuids = load_existing_uuids(PBXPROJ_PATH);

    // ===== 1. 通过纯文本查找，提取原有内容 =====
    // 定义标记
    const std::string build_begin_flag = "/* Begin PBXBuildFile section */";
    const std::string build_end_flag = "/* End PBXBuildFile section */";
    const std::string ref_begin_flag = "/* Begin PBXFileReference section */";
    const std::string ref_end_flag = "/* End PBXFileReference section */";

    // 1.1 寻找 PBXBuildFile 区域 (并提取内部内容)
    size_t build_start = content.find(build_begin_flag);
    size_t build_end = content.find(build_end_flag);
    if (build_start == std::string::npos || build_end == std::string::npos) {
        return -1;
    }
    
    // 拿到中间的内容(不包含注释本身)
    size_t build_inner_start = build_start + build_begin_flag.length();
    std::string build_content = content.substr(build_inner_start, build_end - build_inner_start);

    // 1.2 寻找 PBXFileReference 区域 (并提取内部内容)
    size_t ref_start = content.find(ref_begin_flag);
    size_t ref_end = content.find(ref_end_flag);
    if (ref_start == std::string::npos || ref_end == std::string::npos) {
        return -1;
    }

    size_t ref_inner_start = ref_start + ref_begin_flag.length();
    std::string ref_content = content.substr(ref_inner_start, ref_end - ref_inner_start);

    // 1.3 寻找指定的 PBXGroup 区域
    std::string group_search_key = GROUP_UUID + " /* ";
    size_t group_pos = content.find(group_search_key);
    if (group_pos == std::string::npos) return -1;

    size_t children_start = content.find("children = (", group_pos) + std::string("children = (").length();
    size_t children_end = content.find("\t\t\t);", children_start);
    if (children_start == std::string::npos || children_end == std::string::npos) {
        return -1;
    }

    std::string group_content = content.substr(children_start, children_end - children_start);


    // ===== 2. 批量生成新内容 =====
    const std::string avoid_prefix("12"); 
    std::string build_snippets;
    std::string ref_snippets;
    std::string group_snippets;

	// const std::string prefix = "../../../external/tensorflow/XNNPACK/src/";
    const int add_count = FILES_TO_ADD.size();

	std::set<std::string> existed_filenames = {"f16-f32-vcvt-neonfp16-u16.c",
		"f16-rdmax-2p2x-scalar-c2.c", "f16-rdmin-2p2x-scalar-c2.c"};
    for (int at = 0; at < add_count; at ++) {
        FileEntry file = FILES_TO_ADD[at];
		// file.rel_path = prefix;
		// file.rel_path.append(file.name);

		std::string filename = utils::extract_file(file.name);
		VALIDATE(existed_filenames.count(filename) == 0, null_str);
		file.name = filename;
		existed_filenames.insert(filename);

        std::string uid_build = generate_safe_xcode_uuid(used_uuids, avoid_prefix);
        std::string uid_file = generate_safe_xcode_uuid(used_uuids, avoid_prefix);
        used_uuids.insert(uid_build);
        used_uuids.insert(uid_file);
        
        SDL_Log("UUID: (Build): %s (File): %s for %s", uid_build.c_str(), uid_file.c_str(), file.name.c_str());

        // (1) 拼接 PBXBuildFile
        // (1) PBXBuildFile 片段
        // 215872A23004A8EA0033F4CB /* f16-f32-vcvt-neonfp16-u16.c in Sources */ = {isa = PBXBuildFile; fileRef = 215872A13004A8EA0033F4CB /* f16-f32-vcvt-neonfp16-u16.c */; };
        if (at != 0) {
            build_snippets += "\n";
        }
        build_snippets += "\t\t" + uid_build + " /* " + file.name + " in Sources */ = {isa = PBXBuildFile; fileRef = " + uid_file + " /* " + file.name + " */; };";

        // (2) 拼接 PBXFileReference (使用相对路径 sourceTree = "<group>")
        // (2) PBXFileReference 片段 (使用相对路径和 group，更安全)
        // 215872A13004A8EA0033F4CB /* f16-f32-vcvt-neonfp16-u16.c */ = {isa = PBXFileReference; lastKnownFileType = sourcecode.c.c; name = "f16-f32-vcvt-neonfp16-u16.c"; path = "/Users/ancientcc/ddksample/external/tensorflow/XNNPACK/src/f16-f32-vcvt/gen/f16-f32-vcvt-neonfp16-u16.c"; sourceTree = "<absolute>"; };
        if (at != 0) {
            ref_snippets += "\n";
        }
        ref_snippets += "\t\t" + uid_file + " /* " + file.name + " */ = {isa = PBXFileReference; lastKnownFileType = " + file.type + "; name = \"" + file.name + "\"; path = \"" + file.rel_path + "\"; sourceTree = \"<group>\"; };";

        // (3) 拼接 PBXGroup child (末尾保留逗号)
        // (3) PBXGroup children 插入片段 (结尾带逗号)
        // 215872A13004A8EA0033F4CB /* f16-f32-vcvt-neonfp16-u16.c */,
        if (at != 0) {
            group_snippets += "\n";
        }
        group_snippets += "\t\t\t\t" + uid_file + " /* " + file.name + " */,";
        if (at == add_count - 1) {
            group_snippets += "\n\t\t\t";
        }
    }
	VALIDATE(existed_filenames.size() == 3 + add_count, null_str);

    // ===== 3. “截断法” 重写文件 =====
    // 逻辑：拿原文件的【开头到 BuildBegin】 + 【新Build区域】 + 【原文件的 BuildEnd 到 RefBegin】 + 【新Ref区域】 + 【原文件剩下的部分】
    
    // 3.1 截断并重组 BuildFile 和 FileReference 部分
    std::string final_content = 
        content.substr(0, build_start) + // 取到第一个 Begin 之前
        build_begin_flag + build_content + build_snippets + "\n" + build_end_flag + // 新组装好的 Build 区
        content.substr(build_end + build_end_flag.length(), ref_start - (build_end + build_end_flag.length())) + // 取 BuildEnd 和 RefBegin 之间的原样内容（包含了你截图里的空行！）
        ref_begin_flag + ref_content + ref_snippets + "\n" + ref_end_flag + // 新组装好的 Ref 区
        content.substr(ref_end + ref_end_flag.length()); // 取 RefEnd 之后的所有剩余部分

    // 3.2 重写 Group 区域 (只能在 final_content 上操作了，因为结构变了)
    size_t final_group_pos = final_content.find(group_search_key);
    if (final_group_pos == std::string::npos) return -1;
    size_t final_children_start = final_content.find("children = (", final_group_pos) + std::string("children = (").length();
    size_t final_children_end = final_content.find(");", final_children_start);

    // 替换掉这个 children 括号里的内容
    final_content.replace(final_children_start, final_children_end - final_children_start, group_content + group_snippets);

    // ===== 4. 写回文件 =====
    std::string output_path = PBXPROJ_PATH + ".new";
    // 写入前，强制把 Windows 的 \r\n 转换成 Mac 的 \n
    std::string final_safe_content = std::regex_replace(final_content, std::regex("\r\n"), "\n");
    
    write_file(output_path, final_safe_content.c_str(), final_safe_content.size());
    return 0;
}