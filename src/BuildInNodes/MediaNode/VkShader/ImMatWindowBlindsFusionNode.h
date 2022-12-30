#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <WindowBlinds_vulkan.h>

namespace BluePrint
{
struct WindowBlindsFusionNode final : Node
{
    BP_NODE_WITH_NAME(WindowBlindsFusionNode, "WindowBlinds Transform", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Fusion#Video#Shape")
    WindowBlindsFusionNode(BP* blueprint): Node(blueprint) { m_Name = "WindowBlinds Transform"; }

    ~WindowBlindsFusionNode()
    {
        if (m_fusion) { delete m_fusion; m_fusion = nullptr; }
        if (m_logo) { ImGui::ImDestroyTexture(m_logo); m_logo = nullptr; }
    }

    void Reset(Context& context) override
    {
        Node::Reset(context);
        m_mutex.lock();
        m_MatOut.SetValue(ImGui::ImMat());
        m_mutex.unlock();
    }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        auto mat_first = context.GetPinValue<ImGui::ImMat>(m_MatInFirst);
        auto mat_second = context.GetPinValue<ImGui::ImMat>(m_MatInSecond);
        float progress = context.GetPinValue<float>(m_Pos);
        if (!mat_first.empty() && !mat_second.empty())
        {
            int gpu = mat_first.device == IM_DD_VULKAN ? mat_first.device_number : ImGui::get_default_gpu_index();
            if (!m_Enabled)
            {
                m_MatOut.SetValue(mat_first);
                return m_Exit;
            }
            if (!m_fusion || m_device != gpu)
            {
                if (m_fusion) { delete m_fusion; m_fusion = nullptr; }
                m_fusion = new ImGui::WindowBlinds_vulkan(gpu);
            }
            if (!m_fusion)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_first.type : m_mat_data_type;
            m_NodeTimeMs = m_fusion->transition(mat_first, mat_second, im_RGB, progress);
            im_RGB.time_stamp = mat_first.time_stamp;
            im_RGB.rate = mat_first.rate;
            im_RGB.flags = mat_first.flags;
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
    }

    void DrawSettingLayout(ImGuiContext * ctx) override
    {
        // Draw Setting
        Node::DrawSettingLayout(ctx);
        ImGui::Separator();
        ImGui::TextUnformatted("Mat Type:"); ImGui::SameLine();
        ImGui::RadioButton("AsInput", (int *)&m_mat_data_type, (int)IM_DT_UNDEFINED); ImGui::SameLine();
        ImGui::RadioButton("Int8", (int *)&m_mat_data_type, (int)IM_DT_INT8); ImGui::SameLine();
        ImGui::RadioButton("Int16", (int *)&m_mat_data_type, (int)IM_DT_INT16); ImGui::SameLine();
        ImGui::RadioButton("Float16", (int *)&m_mat_data_type, (int)IM_DT_FLOAT16); ImGui::SameLine();
        ImGui::RadioButton("Float32", (int *)&m_mat_data_type, (int)IM_DT_FLOAT32);
    }

    bool CustomLayout() const override { return false; }
    bool Skippable() const override { return true; }

    int Load(const imgui_json::value& value) override
    {
        int ret = BP_ERR_NONE;
        if ((ret = Node::Load(value)) != BP_ERR_NONE)
            return ret;

        if (value.contains("mat_type"))
        {
            auto& val = value["mat_type"];
            if (val.is_number()) 
                m_mat_data_type = (ImDataType)val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
    }

    void load_logo() const
    {
        int width = 0, height = 0, component = 0;
        if (auto data = stbi_load_from_memory((stbi_uc const *)logo_data, logo_size, &width, &height, &component, 4))
        {
            m_logo = ImGui::ImCreateTexture(data, width, height);
        }
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size) const override
    {
        if (ctx) ImGui::SetCurrentContext(ctx); // External Node must set context
        // if show icon then we using u8"\ue91a"
        if (!m_logo)
        {
            load_logo();
        }
        if (m_logo)
        {
            int logo_col = (m_logo_index / 4) % 4;
            int logo_row = (m_logo_index / 4) / 4;
            float logo_start_x = logo_col * 0.25;
            float logo_start_y = logo_row * 0.25;
            ImGui::Image(m_logo, size, ImVec2(logo_start_x, logo_start_y),  ImVec2(logo_start_x + 0.25f, logo_start_y + 0.25f));
            m_logo_index++; if (m_logo_index >= 64) m_logo_index = 0;
        }
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_MatInFirst, &m_MatInSecond}; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_MatOut}; }

    FlowPin   m_Enter       = { this, "Enter" };
    FlowPin   m_Exit        = { this, "Exit" };
    MatPin    m_MatInFirst  = { this, "In 1" };
    MatPin    m_MatInSecond = { this, "In 2" };
    FloatPin  m_Pos         = { this, "Pos" };
    MatPin    m_MatOut      = { this, "Out" };

    Pin* m_InputPins[4] = { &m_Enter, &m_MatInFirst, &m_MatInSecond, &m_Pos };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device        {-1};
    ImGui::WindowBlinds_vulkan * m_fusion   {nullptr};
    mutable ImTextureID  m_logo {nullptr};
    mutable int m_logo_index {0};

    const unsigned int logo_size = 4676;
    const unsigned int logo_data[4676/4] =
{
    0xe0ffd8ff, 0x464a1000, 0x01004649, 0x01000001, 0x00000100, 0x8400dbff, 0x07070a00, 0x0a060708, 0x0b080808, 0x0e0b0a0a, 0x0d0e1018, 0x151d0e0d, 
    0x23181116, 0x2224251f, 0x2621221f, 0x262f372b, 0x21293429, 0x31413022, 0x3e3b3934, 0x2e253e3e, 0x3c434944, 0x3e3d3748, 0x0b0a013b, 0x0e0d0e0b, 
    0x1c10101c, 0x2822283b, 0x3b3b3b3b, 0x3b3b3b3b, 0x3b3b3b3b, 0x3b3b3b3b, 0x3b3b3b3b, 0x3b3b3b3b, 0x3b3b3b3b, 0x3b3b3b3b, 0x3b3b3b3b, 0x3b3b3b3b, 
    0x3b3b3b3b, 0x3b3b3b3b, 0xc0ff3b3b, 0x00081100, 0x03000190, 0x02002201, 0x11030111, 0x01c4ff01, 0x010000a2, 0x01010105, 0x00010101, 0x00000000, 
    0x01000000, 0x05040302, 0x09080706, 0x00100b0a, 0x03030102, 0x05030402, 0x00040405, 0x017d0100, 0x04000302, 0x21120511, 0x13064131, 0x22076151, 
    0x81321471, 0x2308a191, 0x15c1b142, 0x24f0d152, 0x82726233, 0x17160a09, 0x251a1918, 0x29282726, 0x3635342a, 0x3a393837, 0x46454443, 0x4a494847, 
    0x56555453, 0x5a595857, 0x66656463, 0x6a696867, 0x76757473, 0x7a797877, 0x86858483, 0x8a898887, 0x95949392, 0x99989796, 0xa4a3a29a, 0xa8a7a6a5, 
    0xb3b2aaa9, 0xb7b6b5b4, 0xc2bab9b8, 0xc6c5c4c3, 0xcac9c8c7, 0xd5d4d3d2, 0xd9d8d7d6, 0xe3e2e1da, 0xe7e6e5e4, 0xf1eae9e8, 0xf5f4f3f2, 0xf9f8f7f6, 
    0x030001fa, 0x01010101, 0x01010101, 0x00000001, 0x01000000, 0x05040302, 0x09080706, 0x00110b0a, 0x04020102, 0x07040304, 0x00040405, 0x00770201, 
    0x11030201, 0x31210504, 0x51411206, 0x13716107, 0x08813222, 0xa1914214, 0x2309c1b1, 0x15f05233, 0x0ad17262, 0xe1342416, 0x1817f125, 0x27261a19, 
    0x352a2928, 0x39383736, 0x4544433a, 0x49484746, 0x5554534a, 0x59585756, 0x6564635a, 0x69686766, 0x7574736a, 0x79787776, 0x8483827a, 0x88878685, 
    0x93928a89, 0x97969594, 0xa29a9998, 0xa6a5a4a3, 0xaaa9a8a7, 0xb5b4b3b2, 0xb9b8b7b6, 0xc4c3c2ba, 0xc8c7c6c5, 0xd3d2cac9, 0xd7d6d5d4, 0xe2dad9d8, 
    0xe6e5e4e3, 0xeae9e8e7, 0xf5f4f3f2, 0xf9f8f7f6, 0x00dafffa, 0x0001030c, 0x11031102, 0xcf003f00, 0xd22b8aa2, 0xa8a22820, 0x4ec7776b, 0x1b9da6d2, 
    0x19db986c, 0xf46800ff, 0x4d1a00ff, 0xf40b5cd9, 0xede12f57, 0x51e3fa6e, 0x26a12efb, 0xd13d36ef, 0xf6fb42e5, 0xaef2c71d, 0xa46494a2, 0xa22880ae, 
    0x8a16a08a, 0x5110a828, 0x2ed55645, 0x9a4bc7d6, 0xdd27817c, 0x6d68ee43, 0x68967125, 0x57031dac, 0x36bfbab8, 0xccbbafd7, 0x5079748f, 0x45bdfdbe, 
    0x4582916f, 0x5d49c928, 0x45519400, 0x28fc0e31, 0x68fc28fc, 0x1f446afc, 0x1f851f85, 0x3a7f458d, 0x4f73e95a, 0x759f2cbc, 0x3d34f509, 0xa3f09700, 
    0x077dacf0, 0xf8ee9652, 0xbbcd5ddb, 0xa84c6e7a, 0x72dd7e5f, 0xd686652b, 0xc5c13923, 0xc6d5b428, 0x28fc84d5, 0x68fc28fc, 0x5a8869fc, 0xfca8fc28, 
    0x511404aa, 0x227351f9, 0x4e73e95b, 0x3b20b17c, 0x40efb150, 0x5951b8d2, 0x5efb15fa, 0xcbbd35de, 0x27375d26, 0xc75f9ff0, 0x769db422, 0xc1412ebb, 
    0x752769c6, 0x68c36a70, 0xf2a3f2a3, 0x283a22a6, 0x0a0bbaa2, 0x7a617ce4, 0x646f79af, 0x9e3fb273, 0x00ff7b4c, 0x759dbff5, 0x890e9dd5, 0xeab35a04, 
    0x99645946, 0x0363c3f3, 0x717a9c3e, 0xace43451, 0x514f8e86, 0xd4a62dd6, 0xa36c6fed, 0x00802696, 0x001980c1, 0x64fad0f1, 0xa90c7557, 0x672c293c, 
    0x1f18ea28, 0x470d6a50, 0xb453874f, 0x0a96d336, 0x50af2048, 0xc63aa547, 0xe36c6cd1, 0xd61d49b6, 0x195f8031, 0x314ae1c7, 0x58e06e71, 0x412b8aa2, 
    0x5414450b, 0xf19a2b88, 0x71c9d23d, 0x6427a10d, 0xf7987c67, 0x7feb00ff, 0x8d6ae93a, 0x1a1493be, 0x6c1eea9b, 0xe1f94a92, 0xa7cfc088, 0x4d13951e, 
    0x9aa321ab, 0xae6dd5bd, 0xbb6b2b35, 0xc288a438, 0x0cc06000, 0xf4a1d381, 0x6425bbe2, 0xdb481691, 0x0786ba72, 0x855ad4d8, 0x86566aaa, 0x49c162de, 
    0x04f53204, 0x8bda5952, 0x59b7543b, 0x014e451e, 0xa314677c, 0x7a029b16, 0x10b4a228, 0xc28fc2ef, 0xc68fc68f, 0x56f841a0, 0x1a76bd1e, 0x7b4cad58, 
    0xe4f70ed1, 0xeb00ff77, 0xfcdc3a7f, 0x9db6a56a, 0xf7a2b61d, 0x238f6cde, 0x60c47092, 0xa9f0d367, 0x0d596d92, 0xc53a7718, 0x6fa1cebd, 0xf1666773, 
    0x9c0d8018, 0x9e4e070e, 0x58e5d3d5, 0x80554e07, 0x4c63cf61, 0x166a8cbf, 0x6556dea6, 0x4f198204, 0x2d2dd320, 0xc0b2a5c5, 0x74027925, 0x294aeb67, 
    0x7e930da6, 0x7e147e14, 0x62357e34, 0xa8288a16, 0xad5e5610, 0xd8d2db78, 0xef109b79, 0xe79ca97d, 0xfcad00ff, 0xb4a052eb, 0xe74b5bb3, 0xf24a12bc, 
    0xc30c2739, 0x29c5e903, 0xec34e82b, 0xb87589cc, 0x9ee1efee, 0xf2e109d2, 0x0e020c80, 0x577bfa40, 0x0c49fb40, 0x18609584, 0x2f1a3b12, 0x18dbdf98, 
    0x90a02c24, 0x2a722b43, 0xcd3e680b, 0x3ce6c502, 0xcf897f9b, 0x8d699234, 0xa2285ab2, 0xbf3c91a8, 0xe551f973, 0x9fca9ffb, 0x7e947ef8, 0x793ea51f, 
    0x2fcfd805, 0x7954fedc, 0xa7f2e77e, 0x3ca51ffe, 0xe6852145, 0xde274a08, 0x9ea37420, 0xe5437641, 0x8fca9ffb, 0x54fedc2f, 0x6edd7ceb, 0x68db51f2, 
    0xfe08f9c9, 0xe987bf74, 0xd8859d4b, 0xfedc2fcf, 0xe77e7954, 0x1ffea7f2, 0xe9871fa5, 0x76419e4f, 0xfdf7cb37, 0xfffdf228, 0x157d4a00, 0xd815f21c, 
    0xf4df2fcf, 0xfdf7cba3, 0x5b22f529, 0x322f244d, 0xef135946, 0x798eda11, 0x9707d905, 0xe551faef, 0x3495fefb, 0xbb71cb30, 0x68db89ca, 0xfe08f9c9, 
    0x517bcc94, 0x8cdd21cf, 0x00fffdf2, 0x7fbf3c4a, 0x1c459fd2, 0x9ed815f2, 0x28fdf558, 0xe9afc7f2, 0xe2fce34f, 0x2ace3f8e, 0x57dce539, 0xebb13c63, 
    0x8fe551fa, 0xc79fd25f, 0x1d4bc5f9, 0xd3c2b2ac, 0xf7895d20, 0xfc187589, 0xeed21ccd, 0x2c7f6517, 0x7994fe7a, 0xabf4d763, 0x3049db11, 0xda571863, 
    0x93ba4232, 0x3f2eeafc, 0x97e628ce, 0xf20cd570, 0x47e9afc7, 0x4a7f3d96, 0x14e71f7f, 0x4771fe71, 0xd885bb3c, 0x551e559e, 0xf2db233b, 0xf2db23a3, 
    0xae90e7a9, 0x7954f9c6, 0x6f8fec54, 0xda8ea5ca, 0x91796159, 0xc7fb4401, 0xe6398a81, 0x551e6417, 0x144d551e, 0xcabb4f12, 0x4e46db4d, 0xb7c74705, 
    0x823c47e5, 0x47956fec, 0xf6c84e95, 0xf6c8a8fc, 0x90e7a8fc, 0x14fe915d, 0x8c62147e, 0xe107a556, 0xae2b734b, 0x038d6098, 0xee63d234, 0xb807c683, 
    0x52c5a4f6, 0x4b7dd686, 0x18779150, 0x91854323, 0x3a7004a3, 0xa5de918c, 0x3b2a34bd, 0x5c59d993, 0x5ea47ae8, 0xa8ac1268, 0x53072101, 0x2aaebfd7, 
    0xe7bcdbcc, 0x62499bdc, 0x67a5e34a, 0x6b76785c, 0xd4c9cd28, 0x8093edda, 0xcfc83586, 0xe46acca7, 0x1a770d77, 0xc4caeaca, 0xd32e4300, 0x234ac791, 
    0xc39683a6, 0x0a3f0abf, 0x882a4631, 0xf2a3f217, 0x07818aa2, 0xc0b553e5, 0x00684c8f, 0x63b2bc8c, 0x81f100e6, 0x41b547ea, 0x4b8eb24c, 0xc850a9db, 
    0x15b622d0, 0x1d3890d8, 0xd03bcc47, 0xd6e48eca, 0x9ae8f236, 0xc77b7e8c, 0x7d80ca2a, 0x9ddfebd5, 0x9ca5b8de, 0x94853c93, 0xdf4e2c29, 0x36b4814a, 
    0xffdc90d3, 0xc9b56a00, 0x1f67365e, 0x6ba452fb, 0xca8979b3, 0x210092b2, 0x1e470e86, 0x0e9a96b4, 0x51f9095b, 0x324551f9, 0xc5495107, 0x5a88541c, 
    0x9399b6d0, 0x5891784b, 0x86e392b4, 0x747fd871, 0x9fe2ce1a, 0xf4addba7, 0x6285acf2, 0xbf2b6c55, 0x9c4f871d, 0xb8d0a377, 0xb7b05f6e, 0x57efd1bc, 
    0x72cb32ed, 0x0d005024, 0x6fbf0727, 0x9d3b555c, 0xf152eac6, 0x95e46294, 0x9be6c839, 0xfc0800ff, 0x2f37787a, 0xa3b20ba9, 0xfff30f01, 0x2d495900, 
    0x9c3fdeca, 0x1840a610, 0x3a8e0723, 0x39e4d164, 0x4e8a366c, 0x3183e228, 0x1fade7df, 0xd11cade7, 0xfc8340cd, 0x369dadf5, 0x91549a60, 0x25694507, 
    0x5070ddc0, 0x8d951ef4, 0xbed15fcd, 0x8b9c3fd3, 0x1cc8d674, 0x3a264a80, 0xd020790e, 0xede5868b, 0x1ead0b3e, 0xc97737eb, 0x07461d72, 0xf71c98da, 
    0x58f17dc6, 0xdcb544f7, 0xb3b0c9ac, 0x4cb75492, 0x8b0eb79a, 0xe8ba912e, 0x03316d6b, 0xff20421b, 0x55cdd900, 0xe96bbcba, 0x92ddc1cd, 0x5228cb00, 
    0x519c3170, 0x436c39e4, 0xfed17afe, 0x1ccdd17a, 0x8a4766d0, 0xfde35231, 0x7f8cd69f, 0xfbd5fab3, 0x6211f351, 0xffb8548c, 0xa3f56700, 0xb4feec1f, 
    0x22e6507b, 0x71a918c5, 0x46ebcffe, 0x68fdd93f, 0x45cca1f6, 0xe352318a, 0x8cd69ffd, 0xd1fab37f, 0x8f9843ed, 0xb7a9399a, 0x379aee1f, 0x8f9aee1f, 
    0x980bfb68, 0xa9399a87, 0x9aee1fb7, 0x9aee1f37, 0x1ceca33d, 0xcdd13cc4, 0x00ffb84d, 0xffb8d174, 0xedd17400, 0x21e6601f, 0x6d6a8ee6, 0x8da6fbc7, 
    0x8fa6fbc7, 0x3107fb68, 0x992f151d, 0xed9947ed, 0xc22ecf53, 0x4545ecbb, 0x517be64b, 0xcf517be6, 0xd877c12e, 0xcc978a8a, 0xf6cca3f6, 0x825d9ea3, 
    0x1515b1ef, 0x47ed992f, 0x3c47ed99, 0x62df05bb, 0x3a7f7b2c, 0xeafced31, 0x8fde2f7f, 0xdba7de2f, 0x0c227321, 0x313a7f7b, 0x7feafced, 0x2f8fde2f, 
    0x836c8fde, 0xdb631099, 0x6f8fd1f9, 0x7ef953e7, 0xf47e79f4, 0xc81c647b, 0xcedf1e83, 0x3a7f7b8c, 0xa3f7cb9f, 0xdba3f7cb, 0x5844e620, 0xf56700ff, 
    0xfeec1fa3, 0x54df7fb4, 0x5a55df7f, 0xcffe318c, 0x3d9a5beb, 0x64fabc85, 0xfda7cbf3, 0xa7f0d1a1, 0x7bd829ce, 0xf5fd1fd6, 0xa34c1a5b, 0x5b2152ec, 
    0x9f842cfc, 0x0e3cf4b3, 0x1a2ed3b4, 0x1d2a4db2, 0x2279583b, 0x32924a8b, 0xb4ef0eaa, 0xebf48c1c, 0xe4c3f358, 0x3316495c, 0xe75cb9e5, 0xf4b4e2d0, 
    0x460d6efd, 0x3a366ce7, 0xe755b8a5, 0xf5836cc9, 0x524f15c0, 0xf96b4bb7, 0xd9cd8b23, 0xf7fe1bc1, 0x7bd13c23, 0xa26bcba1, 0xfdd93fa6, 0x3ffbc768, 
    0xd5f71fad, 0x2dd5f71f, 0xbd65c74c, 0x416f1905, 0x51d41b46, 0x9f15f586, 0x7acb20dc, 0x34d1de0a, 0x27d3bbd5, 0xe152ac9d, 0x480bf0d1, 0x460f1c57, 
    0xf5868315, 0x6d78d315, 0xa5d31635, 0xa4f12cfb, 0x182e296b, 0xc61e7872, 0x42bf36aa, 0x1a6493e0, 0xb4aead4d, 0x9c16c7d2, 0x2c03ecb1, 0xe919811d, 
    0x4bac88d5, 0xe6de9e88, 0x8d094e58, 0x07d7e7ca, 0x9a69d115, 0x72adf6b5, 0x5be2ec6d, 0x2a97db54, 0x0bf10700, 0x5e6dacc1, 0x4afdb4c5, 0x6020715b, 
    0x3de720b8, 0x5bbb6946, 0xae95e4a0, 0x416f598a, 0x51d05b46, 0x6114f586, 0xdc4745bd, 0xd21b1b66, 0x52e98d8d, 0x19ed5351, 0x1b7bc43c, 0x0b0dbad2, 
    0x4c475d42, 0xe76b739e, 0x7c50393a, 0x1fe0b8cd, 0xdd755851, 0x46e314f8, 0x26ce38b9, 0x45c07f3c, 0x34db265c, 0x4c2defa6, 0xb5481b1d, 0x0dc94cd6, 
    0x03cb83ac, 0x7305bf2d, 0x5c61bdd3, 0xd4bd35db, 0x4e4c1eb0, 0xc1f59cc9, 0x830e7ac5, 0x4bac43af, 0x617aa42d, 0x978452b5, 0xc0fcb889, 0xebc73518, 
    0xbae911d6, 0x39d1f6c4, 0xdc085264, 0x79464672, 0xb77c52a2, 0x4de14f45, 0xd21b5b19, 0x52e98d8d, 0x19ed5151, 0x1eb9308f, 0x518f8ca2, 0xd94b5151, 
    0xc825e5a0, 0x78d515f5, 0xefd14b5b, 0xd466c9b4, 0x31eb3252, 0x9c625a50, 0x7bc41e60, 0xda5b21d7, 0x12ec830c, 0xa42ce4a9, 0x4655c89f, 0x9cd24c9a, 
    0xeda63657, 0x7584878d, 0x062d9184, 0xf7675cc7, 0xa16718cc, 0x951e2419, 0x96c45dc7, 0xb2c093d7, 0xc8481107, 0x0e167f18, 0x6eba5a33, 0x72a706b9, 
    0xadc18662, 0x1b595e80, 0x8a01cc8f, 0x922dd5cb, 0x189250d3, 0x100cb298, 0x9a91e749, 0x416b8272, 0xd1a429ce, 0xa3a8470e, 0x5454d423, 0x65347b54, 
    0xfafe37ca, 0xa7fafea3, 0x6ade1f6d, 0x35ef8f36, 0xb88acc6b, 0xadeafbdf, 0xe9d29769, 0xcc1bed8c, 0x0bb6d8fb, 0xd0310ee0, 0xd1be2a7d, 0x68a3e6fd, 
    0xcc51f3fe, 0x8d6da586, 0xdd109f4b, 0x066342dc, 0xcae8c948, 0x72b222e7, 0x654926e7, 0x77c9c995, 0xb49d2639, 0xdaa8797f, 0xdcd4bc3f, 0xee2b07d3, 
    0xa3fafe37, 0x6da7fafe, 0x366ade1f, 0x2e35ef8f, 0xd3862b64, 0xd346e3fd, 0xe54be3fd, 0xe58ff7af, 0xdebf9647, 0x3247953f, 0xe20af8ef, 0x35de3f6d, 
    0x6ed44b73, 0x54fead74, 0xb6d8c1ae, 0x1807900b, 0x53953ee8, 0x1fef5fcb, 0x7f2d8fca, 0x7c2a7fbc, 0xd4807ffe, 0x71b1b1ad, 0x68425de2, 0x32862c8a, 
    0xb25d897f, 0x3ec75a3f, 0xa779b16b, 0x92635792, 0x1e4d72ee, 0x7800ff5a, 0x6b7954fe, 0x50f9e3fd, 0x80bf7ee7, 0x13bb3739, 0xa3f1fe69, 0xa5f1fe69, 
    0xc7fbd7f2, 0x5fcba3f2, 0x97ca1fef, 0x0af8ef32, 0x4d7d72e3, 0x9fa63e19, 0xb0d17ab0, 0x88e7d37a, 0xfa6486ee, 0xb5d2d09a, 0x5d21bdab, 0x5dd99161, 
    0x402e15b7, 0x3ee81807, 0xf5604b95, 0xa6f560a3, 0x560a14a7, 0xc59bdad8, 0x14b19cfa, 0xfc933157, 0xfa91ed4a, 0x34cb23d6, 0x3c4fd2ae, 0x2cc74893, 
    0x2e4d72ee, 0xc146ebc1, 0x9ea843eb, 0xe3be73e3, 0x194d7d32, 0xb09fa63e, 0x7ab0d17a, 0xae88e7d2, 0x0a7ae087, 0x19053d30, 0xa9cf35a3, 0x0a7a6098, 
    0xef35d2d7, 0x89768bec, 0x2ca52d2c, 0xfcbaf7fb, 0x95fe00dd, 0x4d339a91, 0x9b862d39, 0x97a4a35a, 0x85c853c6, 0x0f669d46, 0x579cc962, 0x3c8d343d, 
    0x2e982ccd, 0x140327e7, 0x6e9ad1cc, 0xca8d7b52, 0xf4c0704f, 0x0a7a6014, 0x756a4633, 0xee9f3724, 0xee9f371a, 0x82fe321a, 0xa2a0bf8c, 0x00efcbcb, 
    0x6ab87fde, 0xa99396ed, 0xd6b4306a, 0x2b2a6d96, 0x3910246d, 0xb57e8eeb, 0x2be82f4b, 0x976cf0b5, 0xf64da3ab, 0xd2bf5178, 0xed7b2c0f, 0x5ca57f5f, 
    0x323b1923, 0x662715e1, 0xede1ad73, 0xd3b15469, 0xc1f4c066, 0x1666953f, 0x82507060, 0xe9a3573b, 0x411be2c9, 0x3b1adb27, 0x172a40b0, 0x92df8300, 
    0x8b3a7045, 0xe54a1d5c, 0x91ca83a7, 0x3f6466a5, 0x3fce49c2, 0x31c2a95a, 0x2838155b, 0xcfdbcaa4, 0xcf1b0df7, 0x7f190df7, 0xd05f4641, 0x5f5e5656, 
    0xff999079, 0xa3f46b00, 0x947eed3f, 0x46ab3eb9, 0xfed3aa4f, 0x4c00adbf, 0xfab500ff, 0x263ddb55, 0x855000ff, 0xcc8bb5a5, 0xdc493b45, 0x35f107a3, 
    0x6bd5274f, 0xb254f0b8, 0x49308d2e, 0x07ed0720, 0x2bfbcf01, 0x3ba90855, 0x2ef0eb3f, 0x67765211, 0xaa06da30, 0x31db5888, 0xa4d2ae03, 0x71d640fe, 
    0xcf2d3838, 0x4d2fbdd2, 0x25cd5a4d, 0xbdf575fb, 0x575146d6, 0xf5a99f83, 0x51f7bce2, 0xb9525f79, 0x3b116379, 0x7fc8ccca, 0x539c9384, 0x5351149c, 
    0xab4c8a82, 0x4abff69f, 0xe9d7fe33, 0xb4ea934b, 0x47adfa64, 0x6468fdf5, 0x9e7f1425, 0x949e7f94, 0xc77e15c6, 0x2835d445, 0x3c485b5a, 0x4fda56c4, 
    0xf883ab98, 0x3f54eb91, 0xbcee4acf, 0xdca3330c, 0xcff31963, 0x45f82754, 0xc56e1254, 0xcc4e2ac2, 0xc6f0a4e6, 0xda6514b3, 0xed3a90cc, 0x40fe1891, 
    0x078fb2e6, 0xa6d72007, 0x8d7a56e9, 0xeb1b7384, 0x3212b6bf, 0x31f2b188, 0xce6b7dee, 0xd42113f5, 0x4c585aae, 0x18b3d22e, 0x9cdcc6c8, 0x92a29ce2, 
    0xaea2e01c, 0xf38fa256, 0xd2f38fd2, 0xa228cca0, 0xfd2a008a, 0xeaa88786, 0x76b6b490, 0x6d2b6ade, 0x0fae7a27, 0x85ea7de2, 0x04049e77, 0x1c20f7e8, 
    0xfee33c1f, 0x575115f9, 0x52112e76, 0x2f356776, 0x50b8b585, 0x073658bb, 0x7facae5d, 0x82206b20, 0x15c1080e, 0x9e4dfae9, 0x3bc5a7a7, 0xd29b5e5f, 
    0xa8832840, 0xaff5b9c7, 0x874cd439, 0x6269b952, 0xcc4abb30, 0x93c33f63, 0x6c94539c, 0xab283887, 0x8aa2a895, 0xffda3083, 0x68fdde00, 0xde00ffda, 
    0xfdf268fd, 0x3c4a00ff, 0x8ed27fbf, 0x0bf0df65, 0xf7fed786, 0x856f5deb, 0xb3699822, 0x38c2c47f, 0xa6704a03, 0x47f90852, 0xcb4bae38, 0xf228fdf7, 
    0x4a00fffd, 0x7b27cda8, 0xbb721995, 0xa097869d, 0xf348627f, 0x45c200ff, 0x2bb8821b, 0xa95fece6, 0xedb9e6e4, 0xeadee46e, 0x6fb24cb3, 0x271b8053, 
    0x6ac6c0e6, 0xfaef9787, 0xfefbe551, 0xabc53994, 0xab69ca21, 0x00ffda20, 0xda68fdde, 0xfdde00ff, 0xfffdf268, 0xbf3c4a00, 0x99a7d27f, 0xb808fc77, 
    0x7f34df7f, 0x3eb834df, 0xa20f468b, 0x9f10bad2, 0x7e76cdf7, 0xb428960f, 0xac5956a9, 0x02899950, 0xe8be1b46, 0xd10737ae, 0x5af4c168, 0xdc45c9a8, 
    0xdc95cbb8, 0x3aad34ee, 0xee9946cf, 0xe5d3561f, 0xc2c1edca, 0x8ff7ef81, 0xaf2f573c, 0xab7fc75e, 0x971771cd, 0x5e29e0b0, 0xd68c818d, 0xd1a20f7e, 
    0x39b4e883, 0x2987aca6, 0xff0959dd, 0xffd17c00, 0xe0d27c00, 0x3e182dfa, 0x6674538b, 0x47731425, 0xda15c634, 0x62783278, 0x6e4967d2, 0xc7b48861, 
    0xfbc1e401, 0x8b6bb6a3, 0xa36a8ee6, 0x19953b2b, 0xce9dbb72, 0x68d9a58f, 0xda724933, 0xb80212e4, 0xfe6abf2b, 0x966b8e5b, 0xd4a3afd7, 0x62ee8975, 
    0xc45600ff, 0x0ca03e05, 0xa3f9b366, 0x56576e9a, 0xb2ba7243, 0x8ee6280a, 0x280a086a, 0xed0a80a2, 0x6f7113fc, 0x93729116, 0x6919475c, 0x07b8038e, 
    0x28e28aee, 0xb9b38baa, 0xb92b9751, 0x66da68de, 0xf74ea09f, 0xca716b52, 0x10b4910a, 0x3919f1a3, 0xbf5f53ae, 0x67d6528b, 0x1b1184ba, 0x82911410, 
    0x67cd1840, 0xd5954351, 0xacaedc90, 0x15455190, 0xd9ff1f24, 
};
};
} // namespace BluePrint
