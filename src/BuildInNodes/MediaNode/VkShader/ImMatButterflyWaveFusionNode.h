#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <ButterflyWave_vulkan.h>

namespace BluePrint
{
struct ButterflyWaveFusionNode final : Node
{
    BP_NODE_WITH_NAME(ButterflyWaveFusionNode, "ButterflyWave Transform", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Fusion#Video#Shape")
    ButterflyWaveFusionNode(BP* blueprint): Node(blueprint) { m_Name = "ButterflyWave Transform"; }

    ~ButterflyWaveFusionNode()
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
                m_fusion = new ImGui::ButterflyWave_vulkan(gpu);
            }
            if (!m_fusion)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_first.type : m_mat_data_type;
            m_NodeTimeMs = m_fusion->transition(mat_first, mat_second, im_RGB, progress, m_amplitude, m_waves, m_colorSeparation);
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
        float _amplitude = m_amplitude;
        float _waves = m_waves;
        float _colorSeparation = m_colorSeparation;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        ImGui::Dummy(ImVec2(200, 8));
        ImGui::PushItemWidth(200);
        ImGui::SliderFloat("Amplitude##ButterflyWave", &_amplitude, 0.0, 1.f, "%.1f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_amplitude##ButterflyWave")) { _amplitude = 1.f; changed = true; }
        ImGui::SliderFloat("Waves##ButterflyWave", &_waves, 1.0, 18.f, "%.0f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_waves##ButterflyWave")) { _waves = 10.f; changed = true; }
        ImGui::SliderFloat("Separation##ButterflyWave", &_colorSeparation, 0.0, 1.f, "%.1f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_colorSeparation##ButterflyWave")) { _colorSeparation = 0.3f; changed = true; }
        ImGui::PopItemWidth();
        if (_amplitude != m_amplitude) { m_amplitude = _amplitude; changed = true; }
        if (_waves != m_waves) { m_waves = _waves; changed = true; }
        if (_colorSeparation != m_colorSeparation) { m_colorSeparation = _colorSeparation; changed = true; }
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
        if (value.contains("amplitude"))
        {
            auto& val = value["amplitude"];
            if (val.is_number()) 
                m_amplitude = val.get<imgui_json::number>();
        }
        if (value.contains("waves"))
        {
            auto& val = value["waves"];
            if (val.is_number()) 
                m_waves = val.get<imgui_json::number>();
        }
        if (value.contains("colorSeparation"))
        {
            auto& val = value["colorSeparation"];
            if (val.is_number()) 
                m_colorSeparation = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["amplitude"] = imgui_json::number(m_amplitude);
        value["waves"] = imgui_json::number(m_waves);
        value["colorSeparation"] = imgui_json::number(m_colorSeparation);
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
        // if show icon then we using u8"\uf1ee"
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
    float m_amplitude   {1.f};
    float m_waves       {10.f};
    float m_colorSeparation {0.3f};
    ImGui::ButterflyWave_vulkan * m_fusion   {nullptr};
    mutable ImTextureID  m_logo {nullptr};
    mutable int m_logo_index {0};

    const unsigned int logo_size = 4354;
    const unsigned int logo_data[4356/4] =
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
    0x8a16a08a, 0xa200a928, 0xacf07e95, 0x2dea11df, 0xa1e1e9a6, 0xc0489360, 0xf538c029, 0xa479feeb, 0x1ad895dd, 0x872f5634, 0xd98b3b75, 0x586f2d6e, 
    0x65e4a399, 0x30ee3840, 0xb6ce0f3f, 0x4514078f, 0x0aa02bf3, 0x0b98a228, 0x92541445, 0xf5545114, 0xedf441ad, 0x8180363a, 0x4eb5af3c, 0x3c772001, 
    0x4d3a7ffb, 0x45b90cd9, 0x847ae865, 0x974eb1f7, 0x90e2994d, 0xe300c886, 0xd41a3fe8, 0x5d492da2, 0x14455100, 0x51142dc4, 0x96aa0252, 0x4d32aa60, 
    0x96fc683a, 0x19494e0a, 0xd63f2da9, 0x976bf79f, 0xc3c405f1, 0xec46125a, 0xc954b562, 0x32d58e07, 0x29f39a95, 0x1a3a1b2d, 0x04793b2a, 0xe7e0e8b6, 
    0x5649ad23, 0x8b889dd5, 0xa2284cba, 0x4519438a, 0x05c55614, 0xbd303e72, 0xb2b7bc57, 0xcf1fd939, 0x00ff3d26, 0xbacedffa, 0x4487ceea, 0xf5592d82, 
    0x4cb22c23, 0x81b1e1f9, 0x383d4e9f, 0x56729aa8, 0xa8274743, 0x6ad316eb, 0x51b6b776, 0x004013cb, 0x800cc060, 0x327de878, 0x5486ba2b, 0x3396149e, 
    0x0f0c7594, 0xa30635a8, 0xdaa9c3a7, 0x05cb691b, 0xa8571024, 0x639dd223, 0x7136b668, 0xeb8e24db, 0x8c2fc018, 0x18a5f0e3, 0x2c70b7b8, 0xa0154551, 
    0x2a8aa285, 0xc2fb5540, 0x517f3db9, 0xfcf8b585, 0xd48267e5, 0x9e7b5580, 0x557efaa7, 0x8e2afdd5, 0xa745a59f, 0xb270cbdc, 0xfd922c49, 0x723d26e3, 
    0xa7890a7a, 0x9923102d, 0xdb3fd43a, 0x36da00ff, 0xc388c8b1, 0x3cc756e7, 0x0dfcf460, 0x01866076, 0xc830e494, 0x5653b53e, 0xadd5a3d3, 0xa31839d6, 
    0x0e182521, 0xa4ca1f87, 0xd292b7b0, 0x25793bca, 0xd0c69812, 0xd48e63c0, 0xb85b9ca2, 0x5114c532, 0x51b4005a, 0x5c214945, 0x23a8afd6, 0x5207306b, 
    0x0c30dbd0, 0x7f727e07, 0xa996aea0, 0xe99169d8, 0x4b3249f3, 0x79bfbc23, 0x3db91e9f, 0x7b934405, 0x3a36180d, 0x93f03f9c, 0xd6212449, 0x86dba61b, 
    0x00fff1fe, 0x5455d7eb, 0x7bbbf4ee, 0x246e98db, 0xbb1f1edc, 0x7235e3b4, 0x58952baa, 0x280abb1b, 0xa28598a2, 0x9a402a8a, 0x122381d4, 0xd79c0270, 
    0xdcded21e, 0xb421a98b, 0x609061f3, 0xa507eeb9, 0x992f1275, 0x0149c61b, 0x380e0ec6, 0x455b5ba6, 0x12c1026b, 0x27a017e1, 0x35871235, 0xcd7cd8ef, 
    0xc33f232b, 0x98d833d3, 0x62a3619c, 0x69b5d207, 0xca211aab, 0x81631ca0, 0xee64ad4e, 0x0a564aee, 0x62a4a228, 0x67a3f76c, 0x398a3ebd, 0xb0885d98, 
    0x3b6fe7b3, 0x9bd24f46, 0xeeaedeb3, 0x24d974f2, 0xd8ea1f55, 0x3b57fab1, 0x4d93eaa6, 0x1c38c533, 0x523b8e1c, 0xab7c6e53, 0x964fc2b5, 0xd9d5cc3c, 
    0x7acf46ef, 0x98f9147d, 0xefd98c5d, 0x7d7acf46, 0xbb307314, 0x8ddeb31b, 0x2afaf49e, 0xc7ae9879, 0x0ba4ab35, 0x70ac7033, 0x5bd4d433, 0x29dee93d, 
    0x94ed887b, 0x331fe4c4, 0xc7111c3c, 0xb2ae00ff, 0xdc51ed74, 0xcd10732c, 0x1423f7d8, 0xcda79453, 0xf9a8836e, 0xd9d46c12, 0x7acf46ef, 0x11942170, 
    0x668e96de, 0x9ecd304f, 0xa7f76cf4, 0x1d3347d1, 0xe83d9bd8, 0x0c209611, 0x0e007892, 0xd796eab4, 0xffe13efe, 0xfc8bae00, 0xae9879ea, 0xcba649c7, 
    0x67308c1a, 0x550759c6, 0xa432c6aa, 0x504770ab, 0x2bf23e45, 0x694ba6b0, 0xda564756, 0x7bdcb9db, 0xf10b54fe, 0x465db471, 0x0e378ec0, 0x6a7d7d18, 
    0x2a9a5361, 0xd3c65eef, 0xeac94e82, 0x1bbd6733, 0x1449ea3d, 0xe4a08c24, 0x5a1f2029, 0x3133576d, 0xf49eddb8, 0x51a7f76c, 0xb10b3347, 0xc9e93dbb, 
    0x10e1480b, 0x0ad06316, 0xdb81cd2a, 0x31d08778, 0x4dc700ff, 0xa0ad9443, 0x2962b72b, 0x7789a46c, 0x64f73ae4, 0xa1d607ce, 0x7952efd9, 0xc2f6e913, 
    0x75815b7b, 0xe72af312, 0x0dfcf027, 0x2eec193a, 0x6f36925d, 0x373e6333, 0x6320e374, 0x15a2d61f, 0xbd919d9c, 0x682e694a, 0x7b7608bd, 0x9ddeb3d1, 
    0xcecc6945, 0x3dbbb17b, 0x4eefd9e8, 0x17668ea2, 0x8aa26863, 0xa5baa1d0, 0x2374d9c7, 0x0c296c21, 0x8027f5b8, 0xe2729d3f, 0x1b7701f6, 0x00da9547, 
    0xc87f1d39, 0x8c3a74c5, 0x04c5de32, 0x1b47540a, 0xd37b60ee, 0xae2dd327, 0x03645422, 0xc800426f, 0xa9efa3a4, 0xb0f0d7b7, 0xe9dd4fdb, 0x00fff867, 
    0x7d6216c3, 0x78ce29f1, 0x4b6bd5a7, 0x0b05b667, 0xf5c45eef, 0x497235ab, 0xb6e90c3d, 0x4551a8e3, 0x458b2515, 0xad885414, 0x3bda39e2, 0x997114b3, 
    0x07cc108b, 0xe68af1f0, 0xc32d6fda, 0x67986805, 0xe96a8701, 0x5cfabfc5, 0x34cb2597, 0xde51739d, 0x1979c358, 0xf4185057, 0x632a726c, 0xc1290917, 
    0x57e6d7eb, 0x684ddb35, 0x8b3488ec, 0x51c0b4bf, 0xd63a598e, 0xe958638d, 0xfe7d3ad7, 0xe40b0f57, 0x4a4f0f12, 0xef24add9, 0x9566acad, 0xa2280ab4, 
    0x4b6aa1a4, 0x8f988b73, 0xd4f963fb, 0x10648474, 0x813dd47b, 0xb6f4cd15, 0x7c816341, 0xdd9dd4e8, 0x8f9c2739, 0xf2bc8ac7, 0x4a40dd59, 0x0415929d, 
    0x56bdd767, 0xee8b148e, 0x4298f1d4, 0xeb109614, 0x477070d5, 0xa25a00ff, 0xe3dd6074, 0xce37e3a0, 0xf68a24e5, 0x5b104aec, 0x4f825371, 0x7bd552e7, 
    0x33b7774b, 0x1c896c6e, 0x15abf6b0, 0x8290d5ab, 0x30a4288a, 0x00ff6da9, 0x7146e1e3, 0x037f829c, 0x23405051, 0xeec9a606, 0x9856be38, 0x58dcb1cf, 
    0xb84cebe6, 0x32af5037, 0xe14fddc6, 0x53fa83df, 0x99742e6d, 0x636a348a, 0xed86439e, 0x15fe24c7, 0xc1c04434, 0x354825a2, 0x66d8c72d, 0x922594b7, 
    0x83ab3c45, 0xadc78122, 0x0e5fe55a, 0xbfe67086, 0x8ad643bc, 0x0f541b45, 0x52504b41, 0x80a2280a, 0x5349451b, 0xc41dc15a, 0x01c924a5, 0x320e0077, 
    0xb43e3d72, 0x2e744bdd, 0xab48f33e, 0xb2b85c45, 0x1a631792, 0x257e8897, 0xffdebe3e, 0xe4c7e400, 0x3c7ff6c9, 0x28b809a9, 0x669c8c27, 0x699baa92, 
    0x7671510e, 0x4a2a7a65, 0xb9c8b92a, 0x1425151d, 0x51c68573, 0xb9255252, 0xdc86bdbb, 0x8ee15d31, 0x4880d8e1, 0x0e9bf6a8, 0xcab0f33e, 0xdc1ecc29, 
    0x78414eba, 0x9e5ef1fe, 0x496b66b5, 0x9aa0ed2d, 0x04ce974f, 0x8e31ee85, 0xab119dbf, 0x3490aecc, 0x512ab3d3, 0xda4f5152, 0x2a3a7213, 0x81f6284a, 
    0x95549471, 0x9049e15a, 0x0755f5b2, 0x41fbd4f9, 0x454b56c7, 0x842a5f39, 0x0088c502, 0xad499201, 0xf6129d2b, 0x74cfdfd6, 0x6d0b7005, 0xa2f52b39, 
    0x90ae2e35, 0xa9e832b5, 0x579ca328, 0xa2a4a223, 0x1db9708e, 0x93563515, 0xd96e312b, 0xd41ae7a9, 0x4b34157d, 0x5b31f724, 0x257919c3, 0xc52a5038, 
    0x83d585dd, 0x63c2dcaa, 0x82bc322c, 0x5328e20f, 0x7285bdba, 0x1435158d, 0x68c88573, 0x9ca3a8a9, 0xb456472e, 0x90ea45d2, 0x715288bb, 0xd4aad2eb, 
    0xadb5d3b6, 0x66009dc1, 0xd30354d8, 0x34a03f92, 0xb88c9eec, 0xd8dc95cb, 0xbc1de98a, 0xfcad52e5, 0x951e7b4a, 0x9d2be642, 0x032b0d97, 0x1e747519, 
    0xe24f9083, 0x9256e94f, 0xba84be54, 0x7d1f2761, 0xa57a7f47, 0x66716b7b, 0x89b33cd2, 0x5455c10d, 0xff07b5eb, 0x54b45e00, 0xdfd655d4, 0xaa4ae5f0, 
    0xa2ba349d, 0xb25214a5, 0xcdb8c7ed, 0x51949841, 0x3a2d0045, 0x5e5de4cd, 0xda6e1a03, 0x3ff0231b, 0x2da94de1, 0x49bd601d, 0xb80e24db, 0x6afd11fa, 
    0x60b4541c, 0x26aacba5, 0x223e658a, 0xce66756c, 0xf556a93e, 0x084b2528, 0x927db84b, 0x9b9e6338, 0x85fc408e, 0x78939d67, 0xf0283fb2, 0xfe1d86e7, 
    0xa0d66a5e, 0xca866489, 0x65c27c29, 0xed3f6569, 0x15f0831e, 0xf9891c6a, 0x25292997, 0x619ff662, 0x22e9a86d, 0x8c4c3632, 0xd7737c7e, 0x61857ff9, 
    0xe7a5f556, 0xd5ed4bfc, 0x6d296b1f, 0x7f8e7ae5, 0xc42dd6c0, 0x62891b62, 0xa81c2107, 0x4b34b83e, 0x64b2a5a0, 0x49455174, 0x4c49d502, 0xc16456a2, 
    0xb51ec119, 0x83d1986e, 0x16b53e92, 0x695b724f, 0xd8d05868, 0xa76aa736, 0x4dd1affd, 0x6308b676, 0x97368eb2, 0xef1020ce, 0x73b2e59c, 0x465cf1fa, 
    0xe2990781, 0x17f21108, 0x00ff712e, 0x69d53501, 0xdbda59f2, 0xe8d2360d, 0x7282b13c, 0x6b9e5347, 0x5671514a, 0xa35bf766, 0xebf4ed25, 0xb935a8c3, 
    0x186ec931, 0x41001c94, 0x2b3dfdf4, 0xaf2db522, 0xb85bbea1, 0x7b6bb625, 0x07990bb1, 0x3f47deae, 0x5297d55e, 0x5114e4cc, 0x55472245, 0x2fee0359, 
    0xda468e9c, 0x31284cb5, 0xda15a9c0, 0xdbf0d3d8, 0x3c185867, 0xd28f2df9, 0xebdeafb4, 0x6a11b74e, 0xe5ed2f68, 0x4c060e23, 0xea00ff6d, 0x56f866cd, 
    0x5f5a6d35, 0x01096d34, 0xc8418ff9, 0x7b4172ab, 0xb426c6a0, 0x896cd465, 0x0b07cacc, 0x9c00cf81, 0xedf3df71, 0x0bcb0557, 0xbd424f33, 0xf67f1ad4, 
    0x63b3b838, 0x2c8f0b23, 0xe5e749c8, 0x9915f5cf, 0xe9dd725b, 0xc10d9ad7, 0xc530b6b4, 0xefb08a0c, 0x4e70921c, 0xb1ca5f3f, 0xdc32592a, 0x448aa228, 
    0xd94ed191, 0x960d4cef, 0x735aefdb, 0xac1cb722, 0x9094c1c8, 0xee4a7147, 0x626731d2, 0x6d34f7c4, 0x67a319e7, 0x2ee81cbd, 0xdc33d086, 0xd17bb6d3, 
    0x648edeb3, 0xa7684317, 0x67a3f76c, 0x2ec81cbd, 0xb39fa284, 0x483098de, 0x42e6a919, 0xa2dde8b8, 0x55426491, 0xa20ea3d7, 0x4f4e9290, 0xbd6f9726, 
    0x397acf2e, 0x860e5d90, 0x6e27e0e1, 0x448d3d08, 0x8b050032, 0x34f5e41e, 0x6cf49efd, 0x0f99a3f7, 0xfd146598, 0xf76cf49e, 0xe80a99a3, 0xbeed146d, 
    0xea7d9bf4, 0x2aae9079, 0xd1fb463b, 0xc808b68a, 0xa1c183eb, 0xe870a31d, 0x414e59c5, 0xefdba81d, 0x397adf46, 0xbad25c90, 0xd7ea9ed6, 0x708b426b, 
    0x4799d646, 0xf5d33125, 0xdb4e9735, 0x7adf46ef, 0xca8d696e, 0x9da28dfb, 0xbe8ddeb7, 0xae90b9f4, 0x7bb65314, 0xa7deb3d1, 0x45e80a99, 0x58c14076, 
    0x8e0dc38d, 0xfa69ade2, 0x219bce9d, 0x6ce51b68, 0xd5d0436e, 0x6cf49e6d, 0x0f9da3f7, 0x4bddc59a, 0x48c45b52, 0x6b4bb760, 0x3456ce78, 0x38fc1918, 
    0xa13a7fee, 0x46efd94e, 0x696e7acf, 0xd1c69583, 0x46efd94e, 0xc85c7acf, 0x7c554257, 0x8ff1f4fe, 0xa858cdeb, 0x9e5c4999, 0x8e6ba508, 0x4db434f6, 
    0x554f3d3a, 0x547625f2, 0x7a3b9611, 0x6afd719c, 0xcbe1a9de, 0x6b73399b, 0x0912e6ba, 0xcfc97ce0, 0xdcf13b42, 0xbe29f07b, 0xb7b5050f, 0x4ff2c861, 
    0x66eb5af3, 0xfcb0cfc7, 0x1421add9, 0x186976e1, 0x9a1771a6, 0x0984e54d, 0x5161c435, 0x1c1806ce, 0xe9e3781e, 0xd7a5eb54, 0xd162afad, 0x719e232d, 
    0x3be49437, 0x1fe76677, 0x6bfc1890, 0xb392a99a, 0x28cc4a22, 0x8b448aa2, 0xe6dfa750, 0x8132a6a6, 0x1648ad8f, 0xb74febb4, 0xe66d09ea, 0xf3f37b79, 
    0x27c03863, 0x2657e1a7, 0x05abdef0, 0x7ff693c7, 0x78e52737, 0x9ef154ce, 0x1c3fc649, 0x071c5e54, 0xcf0672fb, 0x09fadf5d, 0x65f1f6ae, 0xa078d29a, 
    0xd495cb93, 0x9fee7f85, 0x4e31d25a, 0x92829026, 0xe1791167, 0xdbc646dd, 0xa5c612ed, 0x36b60500, 0x0703e74a, 0x7559edf3, 0xdd3dd3e8, 0xc81e9bb6, 
    0x42d1f596, 0xcd72f3c9, 0xe71838c0, 0xdc9ed79c, 0xcedd92c8, 0x69c4a6f1, 0x8c91aa58, 0x530a9c0c, 0x4b710a49, 0xa2283a62, 0x83a680a0, 0xd4903acd, 
    0xb35e0588, 0xf51bb5d1, 0x69b5b656, 0x6cb09415, 0x1d07b880, 0xe9c8194f, 0xc501a8d6, 0x491bbe77, 0x8ea8f026, 0x59b93c09, 0x00ff235c, 0x6a6eee74, 
    0x17bb1be3, 0x1c3bf308, 0xf587d7ad, 0x695a385b, 0x5e63d9ec, 0x3fb632a4, 0xead55923, 0x59dcb136, 0x276fc1e9, 0x3b203237, 0xea492e9c, 0x953e757a, 
    0x928e97e6, 0xa1f1ce5e, 0xaa186944, 0x3ca38d91, 0x0e1b250a, 0xa2215170, 0xc70c2a8a, 0x22154551, 0xa161bf0a, 0x2da43aea, 0x9ab79d2d, 0xde49db8a, 
    0x9ff883ab, 0xe75da17a, 0x3d3a0181, 0xce0407c8, 0x7c00ff71, 0xbbaba88a, 0x3ba90817, 0xc2979a33, 0x5d28dcda, 0xae031bac, 0x903f56d7, 0x07419035, 
    0xf48a6004, 0x53cf26fd, 0xaf9de2d3, 0x20e94daf, 0x63d44114, 0x9cd7fadc, 0xa94326ea, 0x18b1b45c, 0x3166a55d, 0xcec9e19f, 0x4336ca29, 0xca55149c, 
    0x414551d4, 0x3342ca98, 0xd88a96d6, 0x7f87fea2, 0xf376931e, 0x320617b2, 0x49ea41a1, 0xea59d615, 0x2dac9a36, 0xcd1167f6, 0xe7e01cdd, 0xfa7fbd1d, 
    0xab52c2d5, 0x41523915, 0x52aac11d, 0x6833d268, 0x86e4d4d8, 0x47834ac3, 0x0d17b8b7, 0x5192971b, 0x07ce39c0, 0xacf4c77e, 0x4d122669, 0xf3b6618c, 
    0x683745ef, 0xe31cbbdd, 0x25773215, 0x8aa28ebb, 0x3a5a2428, 0x164845d1, 0xac988bf4, 0x97672875, 0x7076133b, 0x7f047932, 0xa46eee5a, 0xf974d5b4, 
    0xc4b82b56, 0xc950c672, 0x399ebb9f, 0xd29d571c, 0x04086086, 0xf58c7a80, 0xb1b252aa, 0x872e9f71, 0xe8ab6b6d, 0x58aa2dda, 0xc690c9a5, 0x24556608, 
    0x3a7fee16, 0x888b35e5, 0x896bb52e, 0x9631c86d, 0xc7e87003, 0xd2a92603, 0x43410700, 0x9b13f495, 0x45d1c26a, 0x14852415, 0xae205251, 0x6e4841d7, 
    0xf2d62a3c, 0x4522245c, 0x1fb1f778, 0xebbd2529, 0xdbdca58a, 0xceede476, 0x55339e71, 0x2ea36217, 0x2dfa7756, 0xb9919e95, 0x865b5517, 0x70696390, 
    0x034fc600, 0xd7ebc735, 0x776bdd10, 0x68b05b33, 0xe80886d9, 0xf3270070, 0xa6e8b306, 0xca6175e5, 0x51b05677, 0x0e041545, 0x442a8aa2, 0x26f8db15, 
    0x222ddee2, 0x8eb826e5, 0x071cd332, 0x15dd0f70, 0x175551c4, 0x2ea37267, 0xd1bc7357, 0x403fcdb4, 0xd6a4ee9d, 0x231594e3, 0xe2472168, 0xa65c7332, 
    0xa5167fbf, 0x0875cfac, 0x29203622, 0x31800423, 0x87a2ce9a, 0xb921ab2b, 0xa220595d, 0x3f482a8a, 0x0000d9ff, 
};
};
} // namespace BluePrint
