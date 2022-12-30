#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <Doorway_vulkan.h>

namespace BluePrint
{
struct DoorwayFusionNode final : Node
{
    BP_NODE_WITH_NAME(DoorwayFusionNode, "Doorway Transform", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Fusion#Video#Move")
    DoorwayFusionNode(BP* blueprint): Node(blueprint) { m_Name = "Doorway Transform"; }

    ~DoorwayFusionNode()
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
                m_fusion = new ImGui::Doorway_vulkan(gpu);
            }
            if (!m_fusion)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_first.type : m_mat_data_type;
            m_NodeTimeMs = m_fusion->transition(mat_first, mat_second, im_RGB, progress, m_reflection, m_perspective, m_depth);
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

    bool CustomLayout() const override { return true; }
    bool Skippable() const override { return true; }

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin, ImGui::ImCurveEdit::keys * key) override
    {
        ImGui::SetCurrentContext(ctx);
        bool changed = false;
        float _reflection = m_reflection;
        float _perspective = m_perspective;
        float _depth = m_depth;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        ImGui::Dummy(ImVec2(200, 8));
        ImGui::PushItemWidth(200);
        ImGui::SliderFloat("Reflection##Doorway", &_reflection, 0.0, 1.f, "%.1f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_reflection##Doorway")) { _reflection = 0.4f; changed = true; }
        ImGui::SliderFloat("Perspective##Doorway", &_perspective, 0.0, 1.f, "%.1f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_perspective##Doorway")) { _perspective = 0.2f; changed = true; }
        ImGui::SliderFloat("Depth##Doorway", &_depth, 1.0, 10.f, "%.1f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_depth##Doorway")) { _depth = 3.0f; changed = true; }
        ImGui::PopItemWidth();
        if (_reflection != m_reflection) { m_reflection = _reflection; changed = true; }
        if (_perspective != m_perspective) { m_perspective = _perspective; changed = true; }
        if (_depth != m_depth) { m_depth = _depth; changed = true; }
        return m_Enabled ? changed : false;
    }

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
        if (value.contains("reflection"))
        {
            auto& val = value["reflection"];
            if (val.is_number()) 
                m_reflection = val.get<imgui_json::number>();
        }
        if (value.contains("perspective"))
        {
            auto& val = value["perspective"];
            if (val.is_number()) 
                m_perspective = val.get<imgui_json::number>();
        }
        if (value.contains("depth"))
        {
            auto& val = value["depth"];
            if (val.is_number()) 
                m_depth = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["reflection"] = imgui_json::number(m_reflection);
        value["perspective"] = imgui_json::number(m_perspective);
        value["depth"] = imgui_json::number(m_depth);
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
        // if show icon then we using u8"\ue8eb"
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
    float m_reflection  {0.4f};
    float m_perspective {0.2f};
    float m_depth       {3.f};
    ImGui::Doorway_vulkan * m_fusion   {nullptr};
    mutable ImTextureID  m_logo {nullptr};
    mutable int m_logo_index {0};

    const unsigned int logo_size = 8192;
    const unsigned int logo_data[8192/4] =
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
    0xe6e5e4e3, 0xeae9e8e7, 0xf5f4f3f2, 0xf9f8f7f6, 0x00dafffa, 0x0001030c, 0x11031102, 0xe6003f00, 0xc232cdf4, 0xded12a4d, 0xa099d9c6, 0xa4c45842, 
    0xd273b493, 0x6500ffac, 0x0ff4bf69, 0xbf00ffb5, 0x6914fe2b, 0xb207f25f, 0x78af00ff, 0x11f400ff, 0x49d0eb56, 0xf6a78258, 0x00ff9b56, 0xfb5ffb40, 
    0x47e1bff2, 0xff9b56f6, 0x5ffb4000, 0xe1bff2fb, 0xaf96ea56, 0xa69bfd7f, 0x973672cb, 0xd00d8451, 0x491a6ab7, 0x657f075c, 0x0ff4bf69, 0xbf00ffb5, 
    0x7f14fe2b, 0xf4bf6965, 0x00ffb50f, 0x15fe2bbf, 0x8af8a193, 0xecfbfee6, 0x1147b1b7, 0xd51edd74, 0xcffd9e2b, 0xa33474a5, 0x327425cb, 0x9b56f6a7, 
    0xfb4000ff, 0xbff2fb5f, 0x56f647e1, 0x4000ff9b, 0xf2fb5ffb, 0xe856e1bf, 0x5110b2aa, 0x2bc01445, 0xf21fc527, 0xc07f5d2f, 0x5a31f43f, 0x47a985d5, 
    0xb66bae73, 0x91b41cde, 0x9914da87, 0x12e64502, 0xeb382e40, 0x6bbd3dfc, 0x0e4a923a, 0x7fe14fe0, 0x7fad17f9, 0x19fa7fe0, 0xa6e46aad, 0xa6f05493, 
    0xc44dbab2, 0xe46d6d8e, 0x21246154, 0x1e03cc0f, 0x9ebf8fbc, 0x19f5d6b5, 0x514a4aa5, 0x288a02d0, 0xa22840ad, 0x942b008a, 0xb8fcc7f1, 0x4fdb00ff, 
    0xbaba96fd, 0xbbf4ade6, 0xb7ea1b9f, 0xc1724350, 0x6308182d, 0xb0300f8e, 0x4130ce39, 0xacf70cfe, 0x6a28c96b, 0x87acb617, 0xf200ffc0, 0x6c00fffd, 
    0x6af600ff, 0xd593ebea, 0xc1577df4, 0xb9e00852, 0x5aea7c93, 0xa5706b55, 0x8604e342, 0xa3ebf7cf, 0xf6c5aed3, 0xc6c80d9f, 0x1d485064, 0x95d67f8f, 
    0x201ba709, 0x9a45f5bd, 0x00b7a228, 0x00288aa2, 0x47f13eaf, 0x5f378cfc, 0x00fd0ff0, 0x727b7557, 0x5c5376b6, 0x9fd448bf, 0x3a59eda9, 0xf1238537, 
    0x853a9c4d, 0x5e9397c4, 0xb0dcee5c, 0x2369e785, 0x6bd11780, 0x9234119b, 0x75941c4c, 0x7fe13767, 0x7f6d18f9, 0x01fa7fe0, 0x5dac41af, 0xf0dfc163, 
    0xf5b55a8c, 0xf686beb1, 0xce52be3d, 0x09e6ceab, 0x4d3dafd0, 0x49b5862f, 0x16391bb5, 0x0b4fb7e9, 0xc8008ee1, 0x881f383d, 0xe66129fc, 0xa526b09a, 
    0xa26836aa, 0xa602ea8a, 0x2000ff95, 0xf7fa2f7b, 0x4100ff8f, 0x69aa6e15, 0xb207f25f, 0x78af00ff, 0x11f400ff, 0x802de956, 0x85f1912b, 0x2cb86bf0, 
    0x302d6f89, 0x3fdf01f2, 0x5de76ffd, 0xa2436775, 0xfaac1641, 0x26599691, 0xc0d8f07c, 0x9c1ea7cf, 0x2b39cd54, 0xbf92a321, 0x525fedd5, 0xe2d3bcb5, 
    0x0a000392, 0x031940ca, 0xc5e943a7, 0x93ca7077, 0x72c692c2, 0xf581a18e, 0x74d4a006, 0x433b75f8, 0xa460396d, 0x04f50a82, 0x6dac537a, 0x3bcec616, 
    0x63dd9164, 0x9cf10518, 0x17a3147e, 0x45b1c016, 0x0a825614, 0x1080a228, 0x9c58aa90, 0x624d3200, 0x2cd63dfc, 0x3592f8a2, 0xac60a63b, 0x52a81125, 
    0x7f18e5c4, 0x82bed42a, 0x6029ab4b, 0x4813518a, 0x9c9177bb, 0x348ef50e, 0x46275d5b, 0x9654cc93, 0xb6f33955, 0x0c24c300, 0x3556d403, 0x92416b53, 
    0xb4d449fc, 0xe646edeb, 0x7406a738, 0x8528d878, 0x55231090, 0x03f624c7, 0xb456e9b9, 0xa379b12b, 0xb09cc9db, 0x1cf58d5d, 0x753a3654, 0x8f98ee85, 
    0xd22ca425, 0x02ca0506, 0x8ceb48dd, 0x1e3db4d3, 0x62391e4d, 0x76642237, 0xb8cd060c, 0x95d29f3f, 0xf959c938, 0x8a524307, 0x14c4ad28, 0x52004551, 
    0xc7d6eed5, 0xd4739e4b, 0x9edc172e, 0x1de13b05, 0x749d9ef5, 0x13bb12f7, 0x5528931b, 0x2aa300c9, 0xb7be6afd, 0xc1ab4ba5, 0x21720b1c, 0xf2cd6d45, 
    0x777bdc6e, 0x58ac9d15, 0x8efd5958, 0x82126d21, 0x008219ac, 0x3875005b, 0x6df15aae, 0xeba81e5c, 0x3a6b233e, 0x49b7d05d, 0xd1ba2966, 0x92bcd1ed, 
    0x03cfc101, 0xbd32462c, 0x78c9354f, 0xa43dec2e, 0xd1fc84d6, 0x8f3df0b6, 0x7e5c00ff, 0x678726b5, 0x86d37061, 0x84f685b4, 0xd86a3c3f, 0x6a6fefce, 
    0xf1e16bb1, 0xfd35ad65, 0x4d1ccbbc, 0xc0096190, 0xf539b007, 0xa314a5e7, 0xa2059324, 0x1445b3b1, 0x14055057, 0xe71c4051, 0x76c4ae8c, 0xf383da11, 
    0xd823f94c, 0x5df1f57f, 0xdbdbc3af, 0x2ed3441b, 0xd1b6505a, 0x615feac6, 0xf194b5eb, 0xb772add7, 0xd6bf09ba, 0x759ebd23, 0x10c06368, 0xdc23c814, 
    0xa9dad7e7, 0x67495e5e, 0xbc9dbda9, 0x630a6fcf, 0x55db446a, 0x0ee0b972, 0x2216d73a, 0x8af97a32, 0xe8b1f64a, 0x74d5399e, 0xe086b4c7, 0xda52095c, 
    0x58244925, 0xfe015399, 0xb2e51450, 0x577bea33, 0xb0b76897, 0x84a9e245, 0x920b6612, 0x122078c1, 0xc41f1c79, 0xb72beb55, 0xd686bebc, 0x8ab877e6, 
    0x2bc54b50, 0x4880536f, 0x7ee4c1eb, 0xf0506795, 0x303737d4, 0xd9d1664f, 0x483847b4, 0x47ce0986, 0x8c511403, 0x84bb6eb9, 0xe836b755, 0xf51c67a0, 
    0x5046bba2, 0xb6bfb4b7, 0xbb048ab6, 0x04122bb6, 0x7036db04, 0x52bf3f06, 0xfc977a79, 0x00ffdafd, 0xf17f33e0, 0x568a6e75, 0xf5f25440, 0xb5fbf92f, 
    0x66c000ff, 0xebe200ff, 0x8400ff94, 0x00ff52d3, 0xf7bf169e, 0x55f17fc3, 0xc15d5dd8, 0x17d76e65, 0xae717932, 0xaf27d832, 0xfbf3bcd2, 0x00ff5517, 
    0xfde765a0, 0xaef06ff8, 0xb5e5d27a, 0x9abad598, 0x6a9af07f, 0xd7c2f35f, 0xfe6ff8fe, 0x2fbfba2a, 0x9f00ff52, 0x06fc5fbb, 0xb82efe6f, 0x575dec1f, 
    0x9f9781fe, 0xc2bfe1f7, 0x5a1ddbbb, 0x5b6751c7, 0x83c2fc49, 0x8f91362c, 0x7c4651cc, 0x3a0dbbd7, 0x5feae50b, 0xff6bf7f3, 0xffcd8000, 0xe5d1c500, 
    0xf7f35fea, 0x8000ff6b, 0xc500ffcd, 0xa22bbad5, 0xa2280ac2, 0x4b351598, 0x3addecef, 0xcc2fbf5b, 0xeef2f1f2, 0xeb4072c6, 0xa929d7f8, 0x47fbaff8, 
    0x59d0964f, 0x3e665e79, 0xc138377f, 0xab3da607, 0x2bd2c47b, 0x18638fe9, 0x316e2e79, 0x598844e5, 0x7a026b9b, 0xbfe34a0f, 0xfa5f75b1, 0xdf7f5e06, 
    0x0a00ff86, 0xa751ade5, 0x69acc664, 0xfe2b9ee9, 0xb4e2d3ce, 0x976f5e36, 0xc6cd9f9f, 0x8fe94972, 0x5db4e97a, 0xcd5efb57, 0x279f3cee, 0xee36856c, 
    0xf41cd8dd, 0x7fc3b51e, 0xf4bfea62, 0x00ffbc0c, 0x15fe0dbf, 0x746278d2, 0xcffea0d3, 0xee2d59bd, 0x45ba94a6, 0x86a56c2c, 0xd823cf00, 0x54a954fe, 
    0xd0b0d96d, 0x8aa268e9, 0x455110eb, 0xad890114, 0x22fb8f78, 0xc9fe2df1, 0x3e8c6ee7, 0x726f33ef, 0x57fad031, 0xc76bad35, 0x6f31f257, 0xbf5b79e4, 
    0x8f73768f, 0xfe5ae961, 0xf5efd120, 0xd3cd4b9d, 0x1492f3e0, 0xaa774811, 0x726cf9e1, 0x9d1f6647, 0xc200ff65, 0xcf7faf21, 0x4f46fe87, 0xaab82afe, 
    0xe1c6d5d5, 0x34a3ae78, 0x1ee9785d, 0xfffc6d7e, 0xf16f3700, 0xec33c6ed, 0xd1a56b7d, 0xaffd477c, 0x00fff678, 0xc676f264, 0xbb99775f, 0x3de818b8, 
    0x00ff9d6b, 0xff5e4384, 0xfc0f9f00, 0x55fc9f8c, 0x2e2cd25f, 0xd776377c, 0x665fbc9a, 0xafc4d081, 0x90583eb8, 0xf4e4c240, 0x5b9d4a53, 0xefa0af34, 
    0xa8ebd417, 0xa10e56a4, 0x9011e494, 0xb86b697d, 0x12144541, 0x019c2400, 0x9b3980d6, 0x96fd18bf, 0xb07f6bf2, 0x64a4f26e, 0x3863e7dd, 0xaeddcf38, 
    0xded45177, 0x925000ff, 0xdfca53ed, 0x7176978f, 0xc25f0780, 0x35bc2eb4, 0xcddddfab, 0xe6696d79, 0x4b237141, 0x3788f913, 0x82831c2b, 0xea3d3872, 
    0x3544f82f, 0xfff0f9ef, 0xffc9c800, 0x9f57c500, 0x36dabc3a, 0x2af7683e, 0xd8a3da69, 0xb25d116a, 0xfcbc7c19, 0x829cb1a5, 0xd135fe3a, 0xedc7785a, 
    0xfddb9057, 0x229b6f83, 0x383befa6, 0xed7ec6c9, 0x21c27f64, 0x87cf7faf, 0xfe4f46fe, 0xf0b5a52a, 0x7761afd6, 0xa7d5e50d, 0xacbb0597, 0x8798bfb2, 
    0x38c9a96a, 0xda812707, 0xd1e55588, 0x738fe630, 0xb4a1a2b8, 0x80beb7bc, 0x99276d4f, 0x82032419, 0x5e4d9d3f, 0x288a8280, 0xc5670ea0, 0xf6c4bd92, 
    0x0667647a, 0xe38f0567, 0xd16bfd81, 0xb5184bb4, 0x491b5e2d, 0x3739d6d9, 0xf000a164, 0x57faf7a4, 0xd7bd1595, 0xcd3d453c, 0x80a1b184, 0x00fd8efc, 
    0x7d700c1c, 0x9e7676cd, 0xb9b1b733, 0x3a619b4b, 0xdbda994a, 0x0723e314, 0xd7d07182, 0xa4f7889b, 0xaa459d99, 0x4d737d3a, 0xb6e5d3b7, 0x2079476b, 
    0xcbfb109e, 0x43b21b87, 0x3800ca00, 0xda9ebce2, 0xbc47f80f, 0x89856845, 0x7355e359, 0xd41fdcee, 0xe3776957, 0xb1bcaf78, 0x89656670, 0x68aca276, 
    0x02d8a3aa, 0x5b27ae00, 0xe0a875d3, 0x98ebdb4b, 0x019010a6, 0xf5e4aa8f, 0x29bea3fb, 0x4bbb5651, 0x59ade9b0, 0x7455d1da, 0xa77fc1cb, 0x97317241, 
    0xc70eb851, 0xab56ebbf, 0x85e54ed2, 0xfb7b5514, 0x9ab1b2a5, 0x6ac4c8e5, 0x873df748, 0x31604fe7, 0xff7ba5ac, 0x07061e00, 0x00a3d911, 0x1f24e03f, 
    0xe895fed4, 0x5a8c253a, 0xa40daf96, 0x9b1cebec, 0x788050b2, 0x2bfd7b52, 0x2f36bccb, 0xdb3ae42c, 0xcc62fbb6, 0xf9c64525, 0xe319c733, 0x74dc11ef, 
    0xc7d3aaae, 0xa5dcd811, 0xdaadb5cd, 0xdbda994a, 0xc1c83862, 0x1a3ab6e0, 0xe626abf2, 0xbda68c9d, 0xe6da75e4, 0x1d616d97, 0x238fb6ac, 0x58123ac7, 
    0x0e82baba, 0x35781930, 0xb639fee6, 0xea5adc30, 0x46e69371, 0x58223f55, 0x77bdb57e, 0x1c7be1e3, 0x726dcf51, 0x5a3e8763, 0xaea27024, 0x05acf07a, 
    0x6f5feb60, 0x254d1cf1, 0x8856798b, 0xb1195e9b, 0xe41c9cb9, 0xbda33e63, 0xf4f7712a, 0x5cef0541, 0x5bb8b5dc, 0x13ae48ab, 0x8681c8ee, 0x584bc57d, 
    0xfbbd111e, 0xecd69846, 0xc0767b7e, 0xf47400ff, 0x5ebfb5fe, 0xcdd55da4, 0xab349942, 0x742b0f04, 0x0a7f2c45, 0xb2f8617d, 0x49baecf7, 0x700f5b85, 
    0x7bf78f76, 0xe38700ff, 0xc0959d44, 0x9ea9e0b7, 0xb8ba4be7, 0x764bef93, 0x95ef7fec, 0xb144c7ae, 0xe1d5528b, 0x639d99b4, 0x104a3690, 0x524f0a0f, 
    0x5e71a50f, 0x0f3d961b, 0x84da164a, 0xf93b92e8, 0x70acaaa8, 0x75bc0ea4, 0x65bb35c8, 0x3e2dabe2, 0xeead2eed, 0xc19948d4, 0x821cb131, 0x35f4180f, 
    0x7d3e4de4, 0x7c2f794e, 0xcbb4d6df, 0xe52c087b, 0xf57cdeb7, 0x59cdc762, 0x80711050, 0x5e9f0cc0, 0x6df18f2b, 0xa4d0bd90, 0x3ea13b19, 0x3f40fd6a, 
    0xbdd59ad0, 0xfaad85f1, 0x465d9344, 0x8e08c2b1, 0xab28e038, 0x5801bc9e, 0xeae3b5da, 0x47df8296, 0x0f278dbc, 0x00a9f7b9, 0x543d08a6, 0x159c22f7, 
    0x5e1ab4e6, 0x0b4f85fe, 0x46af7dde, 0x80de4f8d, 0x3ae867f9, 0x5cb1957e, 0x5d9e868f, 0x4b9f5a2f, 0x382e23b8, 0xe770e3c1, 0xbaaea88f, 0xf16e4abd, 
    0x5f56183a, 0x9e0def88, 0xdf27338d, 0x7d6af793, 0xcdf4d733, 0xe227576a, 0x75d42489, 0xdd2a6d5b, 0x04c96181, 0xfdfa18f0, 0x51a7f307, 0xc78e20da, 
    0x96680446, 0xd116f500, 0x22e800ff, 0xd80a6fba, 0xa33aea5a, 0x9b1777c3, 0x17ccc21a, 0x1d415e71, 0xd72eaec1, 0xedecd35d, 0xb8b9b521, 0xac113c73, 
    0x42826d72, 0x82830137, 0x6945a617, 0xb03a5e69, 0xcd8d2ed2, 0x90d99bbc, 0x4a5b12a1, 0x003d0e46, 0xcd1eaff4, 0x2473544b, 0x4d47b5f9, 0xd6361de2, 
    0xf2cad2da, 0xb344dcce, 0xe60812ee, 0x3c084633, 0xfe7abd61, 0xdecdd55e, 0x5316b742, 0x2cdbf9c0, 0x841e876c, 0xfca89d62, 0x6dd4b140, 0x962cb6e3, 
    0x84db46f0, 0x2272f656, 0xb9a7d7e7, 0xfbf5a9ac, 0x20799b1b, 0xb875b9b4, 0x5a440a95, 0x460e0009, 0x5c5f4906, 0xb0bdb753, 0x47b4f934, 0x4d59e13f, 
    0xa57ba1a6, 0x65b1f0bf, 0xf40f0efc, 0x89b8a2ae, 0x4300ff52, 0x2fefadd7, 0x98468ee4, 0x089deefc, 0xed0a7be8, 0xafc8c8c1, 0xddd29356, 0x5114968e, 
    0xf95f3555, 0x00ffde03, 0xfa9fbcd7, 0x881ead09, 0x74a0a045, 0xc19f5700, 0xaecce57f, 0x00ff4c4f, 0xb8a2ca2a, 0xda9e4faa, 0x7d5a6315, 0x832bcd47, 
    0x7aada39d, 0x118c0001, 0x289af29a, 0xe84b3ea7, 0x00583d16, 0x55510c3a, 0x03f9af34, 0xbcd77fd9, 0xab08fa7f, 0x44b5da75, 0x30020485, 0x7fae6846, 
    0x07f29fc6, 0x07befe8b, 0xa4d482fe, 0xbfc055f9, 0x458f30af, 0xdb18879d, 0x0aa100ff, 0x0bbda0e4, 0x8111b332, 0xc74abf9e, 0x4baab8a2, 0x1babdc9d, 
    0xfbca1757, 0xa48e8c71, 0x74a53f63, 0xcd93177e, 0xf43f64d3, 0x57d08fd8, 0x4b284505, 0x1e0b5796, 0x800e14aa, 0xfce75a0a, 0x2000ff17, 0xe0eb7f79, 
    0x2be800ff, 0x4577055d, 0x14922bf3, 0x01ea4185, 0xc627aea5, 0x8b18f29f, 0xfec7bdfe, 0x2ea7d484, 0xbc977155, 0xa897374d, 0xc4f4bfc6, 0x65d6d41f, 
    0xd7f3d8bd, 0x83e70876, 0xa2c46acf, 0x4aef65b8, 0x3db9b1e3, 0x004eedda, 0x0340726c, 0xe1a515f4, 0x46cdbc99, 0x62f99f41, 0x912bea4f, 0x59be88a2, 
    0xae7a2c5c, 0x67e09cd1, 0x9fb896d6, 0xc800ff05, 0xf7fa5f62, 0xd712fa3f, 0x97d05d6d, 0x9382b832, 0x3370ce68, 0x795e4beb, 0xdddea77d, 0x5bd297ea, 
    0x0b32cf59, 0xe2680199, 0x3cee0066, 0x27550a64, 0xf522aec8, 0x46ddc6fd, 0xaca467e4, 0xb64d533f, 0x22a48db8, 0x5a8f9141, 0xd5c5fecd, 0x7919e87f, 
    0x1b7e00ff, 0xc5fe28fc, 0x19e87fd5, 0x7e00ff79, 0x812bfc1b, 0x0d0df7da, 0x1122ae39, 0x1c035021, 0xebc719e7, 0xbb71614b, 0xeb19b651, 0x56d48f2a, 
    0xab2ef65f, 0xcb4000ff, 0xdff0fbcf, 0x2ef647e1, 0x4000ffab, 0xf0fbcfcb, 0xb542e1df, 0x901e1aee, 0x34403d40, 0x3a96e7b5, 0xa9a6ed7d, 0x9ccd2563, 
    0x8cb9a1f0, 0x50269206, 0x48c6e14e, 0xfaae43af, 0x0057e773, 0xdf145fac, 0xa3253d1b, 0x1fdcc346, 0xfcbb7f2c, 0x8d1fa75f, 0xe213576d, 0xd6b8a4bb, 
    0x48be0504, 0x04fe2323, 0x8ac27f46, 0x8e46b4b2, 0x4a17fbeb, 0x65a000ff, 0x5ff8fd9f, 0x3e36abf0, 0xa7d4b415, 0xf6e85930, 0x7617c872, 0xc7312adf, 
    0x2ba9f5e3, 0x10f7c17b, 0x3df26a5b, 0x8642d1c4, 0x70a50106, 0x641c39a3, 0x67b58ed7, 0x6c767524, 0x23fcefe7, 0x25baa56f, 0x116b89ee, 0xcafd9571, 
    0xc5117f10, 0xd2c5fe54, 0x6719e8bf, 0x177e00ff, 0xf1b12bfc, 0x4987961c, 0xb56681b1, 0x8719986b, 0xa2fb6697, 0x9e27790a, 0xaef503f9, 0xc768896e, 
    0xe18c1e2d, 0x5dab333c, 0xefd3b49b, 0x1e61281b, 0x00062a48, 0xf11c741c, 0xaeaaabfc, 0xb550e706, 0xe62a8d5b, 0x826442da, 0x026cd628, 0xfd1f5730, 
    0x32ea5d71, 0xcaa92bba, 0xeb35c8b0, 0xea587ad2, 0xad37d762, 0x36f16f5e, 0x71ac6695, 0xe06ec41d, 0x6358321e, 0xd175fef4, 0x134b1592, 0xae490680, 
    0x6fe5d606, 0xc90d513c, 0x46dfb36c, 0xdde18f57, 0xa9ea00ff, 0x0b1bedae, 0x6615eaa1, 0xd4ba4fc7, 0x455a30a7, 0xb11448e6, 0xf8f1821b, 0x5bad7a9f, 
    0x38960dfe, 0x571a8975, 0x80dd0644, 0x7202c02e, 0x2a26af38, 0x552cc7ee, 0xdddb8cdd, 0x4f00ff22, 0x60ee3982, 0x0a8714db, 0x9fc0eaea, 0x57a926a8, 
    0xa7caad4d, 0xcdd076c3, 0xe3796bf6, 0xb6b7ed9c, 0x23ed3294, 0xe70910ef, 0xebfdfd3e, 0x6625a799, 0x0e3d4912, 0xb9bf5713, 0x2465fcd3, 0xd48264be, 
    0x71d781ca, 0xa0964f08, 0x1020d3f1, 0x33eaaa6b, 0x2e8ba75c, 0xf6b70c2d, 0x5bb9f8ce, 0x07179194, 0x3c20e591, 0xabfcf5f4, 0xfbba416f, 0x216f8d5e, 
    0xb1abcc6d, 0xbde2b88f, 0xc7d23b3c, 0x7951f84a, 0x9d8d63dd, 0x244115ce, 0x9fb9c29e, 0x7bc9dd07, 0xe4a98be2, 0x3e30df91, 0x573be7dd, 0x9c3e907a, 
    0x6e8a87d6, 0x228bb6c5, 0x424cc386, 0xd25f7f0f, 0xa03178a8, 0x272cc23a, 0x93086c25, 0x01f3e28f, 0x6254cc3f, 0x7c09965e, 0xf62aee2c, 0xaccfa493, 
    0xbbbd3ddd, 0x97918cc6, 0xc0404226, 0xd6077620, 0xe04bd7a8, 0xe0669d61, 0x56f69301, 0xb504fee3, 0xd85dc5e7, 0xb1bb8ae6, 0x17e8a99b, 0x77bc5d7a, 
    0xd004c932, 0x9044dbc8, 0x789ee1be, 0x660d3de8, 0xc2ac6157, 0x213df8d0, 0x292b82bf, 0x04c37cd2, 0x5edf902d, 0xfddd624f, 0xd1c7953e, 0xab396625, 
    0xa4f18433, 0x8af83197, 0xa9246913, 0xc61cb421, 0x313712c4, 0xaf953e3d, 0x7de3ebe1, 0xc5eec4a3, 0xc9914fa4, 0xe78e2439, 0xae41c5f0, 0x78d9d95d, 
    0x6dbd25b5, 0xa00d9bb1, 0xe79227ed, 0x9735dbd3, 0x8f941be1, 0x49b4ba51, 0x68fe8909, 0xfc07dff3, 0xec0ebf2b, 0x0fd19997, 0xfdba1b85, 0xe7d1b2d9, 
    0xb0a35c91, 0x1c1c84d8, 0x81674a9f, 0xaed1a41d, 0x35b3d81d, 0xe4c412d3, 0xac79b593, 0x235d187f, 0xb3d95a5d, 0x93ef14e1, 0xd30f3cdb, 0xe7e0bb35, 
    0xe71b4b8e, 0x5f53fd87, 0x718c5348, 0x294ec585, 0x099f9adc, 0x8f9656d0, 0x46ebdca1, 0x29246d61, 0xcd5b80e4, 0xd0e73a62, 0xaecd4a1f, 0xbd3cc0bf, 
    0x547ec6f8, 0x57efd7e3, 0x8cdd5514, 0x99959320, 0x6a78a88f, 0xfbb34efb, 0xdb133c63, 0x371992f7, 0x813bce01, 0x2bb2d2df, 0x71e1d5b9, 0xe6880be1, 
    0x58cdfeb0, 0x2c714c5d, 0x61f940c2, 0x5c531fd7, 0x312b3935, 0x0e3d29ce, 0xd16ec76b, 0x8ceed6e8, 0x48e99655, 0xda417060, 0x857fd6dc, 0x9624ee2f, 
    0x59f2cee6, 0x8721645e, 0xd891c49a, 0xd50a7f8e, 0x1dc78bf1, 0xfecd8b8d, 0x2fa36fa9, 0x66c378c6, 0xadd496b9, 0xbc628b07, 0x32f994b4, 0x23382490, 
    0xf8ebc193, 0xbbc3ea1a, 0xf8539b2b, 0xb9b9d64e, 0xd6b63e7c, 0xd4ed95fa, 0x902c75a9, 0xc6632ebb, 0x327282c3, 0xcfdd6f4d, 0xae395bf6, 0xa3f2ee36, 
    0x679cdb67, 0x4eae3803, 0x6a3fc6ef, 0xecdf9ab3, 0xd9d87c1b, 0xc6d97937, 0x6bf73346, 0xdc8aa3b2, 0x00ff1ad1, 0xffdd95f0, 0x3f28cf00, 0x34fe93ef, 
    0xec45f1e4, 0x1686848d, 0xf84f813e, 0xa0fd19d7, 0x6afddcdf, 0x85577548, 0xc6f308cb, 0xdc79f53e, 0x8e668f88, 0x7d4ffcc1, 0x0ac1371e, 0xbf55a6e7, 
    0x00ff99c6, 0xfcdf5d09, 0xf9fe83f2, 0x9c5ce33f, 0x6bbcb39a, 0xaf6a8b18, 0xf617d56d, 0xf5737f83, 0xb30791a3, 0x1b345a47, 0xf6c3434d, 0x044591a8, 
    0xe76d0ccd, 0xc039632c, 0x5a737dce, 0xd9ab75ba, 0x246d4558, 0xda1053a2, 0x3b322e18, 0xe3692f57, 0xc3d9b21f, 0xde0df66f, 0xbc9b6a54, 0x190367ec, 
    0x5ad6b5fb, 0x38abf64f, 0xf3ed366e, 0x9cdb5763, 0xaf3823e3, 0x6d2f0e46, 0xa06f63cb, 0x58caea92, 0xd2449422, 0x67e4dd2e, 0xb773bd03, 0x0d3a645a, 
    0x4998849d, 0x66e44ba6, 0xc3d26e93, 0xc7f38c93, 0x6aadb315, 0x6916d9bf, 0xdbf9e471, 0x76b709a4, 0xa1e7c4de, 0x5757aef4, 0xa5f62ff1, 0x93fd50bc, 
    0x12e5f2ca, 0x9cddcc67, 0x7ad03102, 0xb371d5d4, 0x9f3a704f, 0xfe574bf8, 0xf200ff7c, 0x5a00ff2f, 0x1eb07894, 0x00ff5996, 0x6afdbfb6, 0x5bb47fe1, 
    0x00ff79fe, 0x1a2cd5e3, 0x84e540bb, 0x7b1fe359, 0xffbcd21f, 0xfd8c6600, 0xe2533b92, 0x11d93ab0, 0x3f6d00ff, 0xf09fd4fa, 0xf9fcaf96, 0x5fe400ff, 
    0x9271b5fe, 0x42242f6b, 0x39381933, 0xf24f3ecb, 0x5bb47fa8, 0x00ff79fe, 0x347bd4e3, 0x9d1dc91e, 0xdb7a56ac, 0x67cfdc5f, 0xda266911, 0x5166280b, 
    0xe2e0a0e5, 0xf2e8a19f, 0x11cbf168, 0xb02313b9, 0xc16d3660, 0xcd95fefc, 0xfe277ee9, 0xcd4882cd, 0x4dbe9b9f, 0x8e31f3f9, 0x6a4fc700, 0x575d34ea, 
    0xeecd5efb, 0x6c279f3c, 0xddee3685, 0x1ef41cd8, 0x924bdfb5, 0xd62e75c9, 0x88c63a44, 0x0cc4b5fa, 0x8b2121d7, 0xbb337bac, 0xa5cf799e, 0x3de69a53, 
    0xedc1c40f, 0x59c3e26c, 0x2c9fa260, 0x0778fd72, 0xa3f62a9e, 0xded95dab, 0x2e5d7b0b, 0x87dddf6b, 0x2e3f8b66, 0x291c0049, 0x517faef4, 0x00ff5a7d, 
    0xa2b15b50, 0xbc88475e, 0x1e5bbebd, 0x7d3be7e4, 0x59382bea, 0x56f5a0ae, 0xfc00ff66, 0x00ffb325, 0xfc2f3ad0, 0x45fc3f09, 0xcf96f01f, 0xe84000ff, 
    0xff24f0bf, 0xcc15f100, 0xfdbf3af9, 0xef3f2f02, 0x49857fd3, 0x72a1d500, 0x7cbb45c7, 0xdfc4608c, 0xf2714dfc, 0x2792cf44, 0x0900ff45, 0xf400ff6c, 
    0x00ff8b0e, 0x00ff4f02, 0x25fc4711, 0xd000ffb3, 0x09fc2f3a, 0x7345fc3f, 0x226975cc, 0xd125fada, 0x16e2f06c, 0x452dfb1f, 0x00ffeae4, 0xffbc08f4, 
    0xfe4dbf00, 0x39447214, 0x73436d22, 0x62e2b91e, 0x617156f7, 0x8f11b02c, 0xbc7e3898, 0xc953eb81, 0x625dd4e1, 0x44d2422d, 0x31e311b7, 0x5e1f7c2c, 
    0xb1226384, 0xacd5e774, 0x3abb0635, 0x8b99e425, 0xb0e5dbcb, 0xb7730eee, 0x3b0dbada, 0xbcbcbb56, 0x5c5af736, 0x2ebb5fd6, 0x6c7e2ead, 0x51190810, 
    0x5c945deb, 0xe65e5439, 0x08114d8b, 0x5a97f8f0, 0x44eefe92, 0xd8ce46b8, 0x61073e9a, 0x55fe9cd4, 0xdbac8b1e, 0x16da3f69, 0x1f66d6c6, 0x92af906d, 
    0x001c2810, 0x5ae96139, 0x5da9adb6, 0x1d7343da, 0x9149a8ad, 0xb65c435d, 0xfe3c2340, 0x850fb2ee, 0xf23c6375, 0x741eb386, 0xd63c26ad, 0xdca727e3, 
    0x9c4eebac, 0x4e229395, 0x53b3592d, 0x5fcb12fe, 0xfcbbf2f9, 0xe200ffa3, 0x18e3c9e9, 0x5bcbce23, 0xa55dcfd5, 0x59cffe07, 0x6a8bf03f, 0xb3df00ff, 
    0xadbf00ff, 0x54c400ff, 0x53677891, 0x678bf95c, 0xfd7e7a26, 0xcaf13fc6, 0xb448b4e5, 0x84f1270d, 0xd65e1012, 0xedfdb1f5, 0x6700ff87, 0xc200ffa6, 
    0x00ff6b59, 0x947f573e, 0x505dfc7f, 0x8b5ac393, 0x443b122e, 0x7368bf3e, 0x54b200ff, 0x6a8bf05f, 0xb3df00ff, 0xadbf00ff, 0x51c400ff, 0x3ba08568, 
    0xb56dd65a, 0x61fbec6f, 0xb68f326b, 0x08ce5bc6, 0x1c418e54, 0x75a9f531, 0xdfda0d5f, 0x916e8bc4, 0x4bb63a5a, 0xe8914324, 0x78808a71, 0xc1135657, 
    0xe4336b29, 0xcd63b2ca, 0x7d7a706e, 0x75f2dcca, 0xb3f6f95f, 0xa6c000ff, 0x75e300ff, 0xa37446d3, 0x8ccba416, 0x339395a2, 0x0d7f43fc, 0x4a2a9596, 
    0x34c2b518, 0x2a771448, 0x5fd53e79, 0xd21b7ec3, 0x6b1bf46f, 0x324fabab, 0xbcb9dd67, 0x10c319c7, 0x56d00738, 0x6b658a57, 0x475a50fd, 0xb3b0b68d, 
    0xe3c6f7b1, 0x475d01fa, 0x57e43f84, 0x8100ffb3, 0x6de800ff, 0xfa4e2646, 0x74d44605, 0x3444f80f, 0xfff0f91f, 0xffcfc800, 0xff51c500, 0x83860800, 
    0x1f3e00ff, 0x00ff19f9, 0xaedaaaf8, 0x7616c1a7, 0xf67b6fd7, 0x2a67688b, 0x814ab4a9, 0xaef579b0, 0xaedde444, 0x3b291763, 0xf8cff35c, 0xf91f3444, 
    0xc800fff0, 0xc500ffcf, 0x0800ff51, 0x00ff8386, 0x19f91f3e, 0xaaf800ff, 0x4e67ddf4, 0xb2c4f074, 0x905669dd, 0xd34609dd, 0x571b1867, 0xfffc498e, 
    0x28e34a00, 0x39ea3297, 0xcea94739, 0xd1167e6a, 0xb9abf46d, 0x24dbb2e2, 0xf3a13b70, 0x2485e05c, 0x0e9aaa77, 0x1d7367bd, 0x22ab9cb6, 0xcb8590cc, 
    0x03951400, 0x1dbae638, 0x7740fe6b, 0xedf500ff, 0x6b82fe27, 0x68b3c5cf, 0x987ad99f, 0xcf8d12c9, 0xf7df7e8f, 0x3c9caec9, 0x69cdaea4, 0xfea6d636, 
    0x6a0b7fb3, 0x039e66fa, 0x19f25619, 0x06cac749, 0x9ad7c748, 0x61cdabeb, 0x4bfcef91, 0x97c7a316, 0x5fa3f8f6, 0x17fd787e, 0xa84aaff5, 0xe4ca3bc4, 
    0x1405dd55, 0x0ce06357, 0xf576d4ed, 0x00ff78c4, 0xab08ebc7, 0x3cc619bb, 0x451dc7ce, 0xbddd157a, 0xbb1afefc, 0x8eb6d8fe, 0x620c957b, 0xf14a2e11, 
    0xf3d0e3c1, 0x12ed79c5, 0xf228878d, 0xba167185, 0xa89ba6f5, 0x0bb716ea, 0xe52fe92e, 0xd006ca6d, 0xfb3cc709, 0x6d5e6f57, 0x3f8de9a8, 0xe4f8cdf6, 
    0x00e07ac5, 0xfe396e3b, 0xb6c3b662, 0x962edaa4, 0x2b34ebec, 0x570d75f8, 0x6d437850, 0x96882289, 0x3969481d, 0xedd6dc1e, 0x81021e72, 0x7fb75be1, 
    0x38916cf9, 0xfd4166fa, 0xaaa8af2b, 0x8a6c9bdb, 0xb10a139f, 0x7679616f, 0x27aded85, 0x68c14199, 0xfc012ce3, 0x9e77bd2a, 0x3df6cf04, 0x79bedece, 
    0x2af2fdc7, 0x28ecae22, 0xe3d89947, 0xadafd326, 0x77f67c93, 0x9ef72f11, 0x8afa5126, 0x63a35ead, 0x95fda5fd, 0x51bcf637, 0x88cd2fcb, 0x5cee00e2, 
    0x5acff474, 0x8d72eaf2, 0xd8f23887, 0x7ac4032b, 0xfd4d1a9c, 0x8cacf394, 0x02c814ad, 0x19734e00, 0x6fa547ee, 0x8de239d7, 0xfa4efb3b, 0x2ce00cc2, 
    0x90fcb153, 0xaa8ad40f, 0x0a56e737, 0xb435127f, 0xd801f95f, 0x7fb4d77f, 0xb5ab08fa, 0x00ff454b, 0xfd871d90, 0x00ff477b, 0x59bb8aa0, 0x857bc9bd, 
    0xbcb0b758, 0xd6f642bb, 0xe0a0cc93, 0x009671b4, 0xbb5e15fe, 0x740202cf, 0xf93a907b, 0xc8f71fe7, 0x617715a7, 0xc7ce3cc2, 0xaa8ffd1f, 0xdd86f90f, 
    0x86df00ff, 0xa60a00ff, 0x60040741, 0x26fdf48a, 0x8ab456df, 0x2319ed7f, 0x3106c8b9, 0xf531b313, 0xa507361f, 0x08a2ce79, 0x0973aed4, 0xf2b13287, 
    0x27a8e0ca, 0x4639c538, 0x95c3a9c8, 0x1445ad5c, 0x619e1954, 0x99fa3714, 0x645875d4, 0xe6af4044, 0x8e54ea6f, 0x717b7054, 0x3f84af5d, 0xffb357e4, 
    0x00ff8100, 0xea516de8, 0x931e4751, 0x9702517e, 0x3a10cb81, 0x5c73a49d, 0xdfaa93d6, 0x439169db, 0x1ac5eb05, 0x4284c2ee, 0x52f72407, 0x0b11bb6b, 
    0xaea98e24, 0x6e1d7a64, 0xb55b67f8, 0xdad78dd1, 0x2a136792, 0x7100e581, 0xeb91fa8c, 0xb7fd575e, 0x4ad07faa, 0x1ff7fb5f, 0x4a1300ff, 0xf458ad35, 
    0xf63f65d4, 0x26fe3fca, 0x775a54b9, 0x77925032, 0x7b8dac47, 0x79b4d34d, 0x6e9874ec, 0xbd135c4b, 0x08f0b6ee, 0x5cd3c6c1, 0x5a2771e5, 0x465d87d5, 
    0x6c00ff51, 0xe200ffa3, 0xf5b63f69, 0x4b09fa4f, 0xe37e00ff, 0x68e200ff, 0x241c6c71, 0xff5aabce, 0xff1d9000, 0x497bfd00, 0x9aa000ff, 0xcdb46ee1, 
    0x290e7d66, 0x4b86b9e6, 0x1259a258, 0x7195f731, 0xbda7effe, 0x35d6bc5a, 0x8ea76c29, 0x1e19f94b, 0x38a60c36, 0xe17404f9, 0x86d2a16b, 0x7f307b74, 
    0xf43fdde7, 0x7a585d11, 0x9466ba77, 0x2d398ad3, 0xdbf6c236, 0x49e22efb, 0xdfd3ec13, 0xef83ca44, 0xa707cf6e, 0xd2eb3da0, 0x99d7956b, 0xe6f4ad2c, 
    0x1b12308e, 0xe04588d8, 0xb38a6760, 0x580900ff, 0x3f9f00ff, 0x00ff8bfc, 0x10f1acd6, 0xd44492b4, 0x860ebd4d, 0xeb0a1fba, 0xde427a56, 0xfb99df2d, 
    0x2e7681f0, 0xf3cf6e7a, 0x00ff3daf, 0xcf7fac84, 0xff45fe9f, 0xff51eb00, 0xff580900, 0xfc3f9f00, 0xd600ff8b, 0x779212ac, 0x74272921, 0xbf8e7d7a, 
    0xb376e8a1, 0xcede8225, 0x6bf98ff2, 0xfa74648c, 0x912bec75, 0x12fe7bae, 0x3e00ffb1, 0xff17f97f, 0xfc47ad00, 0x7cfe6325, 0x2ff200ff, 0x865a00ff, 
    0x3ea9c1a4, 0x175c4387, 0x7dad7ef6, 0x9da5aa71, 0xbddb31cc, 0xfde8c8e4, 0xc23fcf49, 0x3fb7627b, 0xf31f2be1, 0x9100ffe7, 0x07d5fa7f, 0xe3f36587, 
    0xdfc626d4, 0xc646fa32, 0xd7e08c73, 0x95171a46, 0x57d37499, 0x2c8527b9, 0x2e2eecee, 0x8e24efed, 0xd9615846, 0xc95ca0b3, 0xcd77d081, 0xc9ce9574, 
    0xeba545ab, 0x4d90f913, 0x166f992f, 0x61bcbd3c, 0x3d92eba4, 0x4bf85f6a, 0xcae77f2d, 0xff8ff2f3, 0xebac8b00, 0x6247fb45, 0x73e51667, 0x00ffba76, 
    0x7ddaea08, 0x0d71978e, 0xefc2d2dd, 0x7a102029, 0xe635036d, 0x5a96f07f, 0x95cf00ff, 0xff1fe5e7, 0xfc471700, 0xf3bf9625, 0x47f979e5, 0xd6c500ff, 
    0x8a3b4d71, 0xb177512a, 0xee453aea, 0x66c8e18b, 0xe13dab61, 0x46221f93, 0xeb414f47, 0xcd18d7f8, 0x1963cbb9, 0xff60c539, 0x6b59c200, 0x573e00ff, 
    0xfc7f949f, 0x96f01f5d, 0xcf00ff5a, 0x1fe5e795, 0x431700ff, 0xd4c0d2bb, 0x6b37969e, 0x1724f19f, 0x2659dff3, 0xca70349f, 0x71969422, 0xc7c7dcc6, 
    0xff14df43, 0x2d4bf800, 0xf3cae77f, 0x00ff8ff2, 0xd5a3a88b, 0x8835d5a2, 0x8b26b83c, 0x3e97b7cb, 0xd172de66, 0x953ec1f4, 0x68bf18a5, 0x23b50887, 
    0x00ff4557, 0xfd871d90, 0x00ff477b, 0x58bb8aa0, 0xa6ad4efa, 0x4759a3c7, 0x8e6aa326, 0x5686f1b6, 0x28044199, 0xbfd58ce0, 0xfe2baded, 0xf77f7682, 
    0xb2c67ffd, 0x9d86dc69, 0xf8dbb5cb, 0x2ddee226, 0xb826e522, 0x1cd3328e, 0xdd0f7007, 0xdb7fe715, 0x04fd575a, 0xef00ffec, 0x8d00fffa, 0x575adb1f, 
    0xffec04fd, 0xfffaef00, 0xb6388d00, 0xe216c79d, 0x74d263ef, 0xd04f336d, 0x35a97b27, 0x4805e5b8, 0xf85108da, 0x29d79c8c, 0xa9c5dfaf, 0x42dd336b, 
    0x0a888d08, 0x0c20c148, 0xb67fb066, 0x09faafb4, 0xdf00ffd9, 0x1a00fff5, 0xafb4b63f, 0xffd909fa, 0xfff5df00, 0x6d1b1a00, 0xdab62159, 0x2ada45b2, 
    0x95d6f697, 0x3b4100ff, 0xbffefb3f, 0xd6f647e3, 0x4100ff95, 0xfefb3f3b, 0x6653e3bf, 0xd9ff9945, 
};
};
} // namespace BluePrint
