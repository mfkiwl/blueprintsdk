#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <WaterDrop_vulkan.h>

namespace BluePrint
{
struct WaterDropFusionNode final : Node
{
    BP_NODE_WITH_NAME(WaterDropFusionNode, "WaterDrop Transform", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Fusion#Video#Shape")
    WaterDropFusionNode(BP* blueprint): Node(blueprint) { m_Name = "WaterDrop Transform"; }

    ~WaterDropFusionNode()
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
                m_fusion = new ImGui::WaterDrop_vulkan(gpu);
            }
            if (!m_fusion)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_first.type : m_mat_data_type;
            m_NodeTimeMs = m_fusion->transition(mat_first, mat_second, im_RGB, progress, m_speed, m_amplitude);
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
        float _speed = m_speed;
        float _amplitude = m_amplitude;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        ImGui::Dummy(ImVec2(200, 8));
        ImGui::PushItemWidth(200);
        ImGui::SliderFloat("Speed##WaterDrop", &_speed, 1.f, 100.f, "%.0f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_speed##WaterDrop")) { _speed = 30.f; changed = true; }
        ImGui::SliderFloat("Amplitude##WaterDrop", &_amplitude, 1.f, 100.f, "%.0f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_amplitude##WaterDrop")) { _amplitude = 30.f; changed = true; }
        ImGui::PopItemWidth();
        if (_speed != m_speed) { m_speed = _speed; changed = true; }
        if (_amplitude != m_amplitude) { m_amplitude = _amplitude; changed = true; }
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
        if (value.contains("speed"))
        {
            auto& val = value["speed"];
            if (val.is_number()) 
                m_speed = val.get<imgui_json::number>();
        }
        if (value.contains("amplitude"))
        {
            auto& val = value["amplitude"];
            if (val.is_number()) 
                m_amplitude = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["speed"] = imgui_json::number(m_speed);
        value["amplitude"] = imgui_json::number(m_amplitude);
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
        // if show icon then we using u8"\ue4b0"
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
    float m_speed       {30.f};
    float m_amplitude   {30.f};
    ImGui::WaterDrop_vulkan * m_fusion   {nullptr};
    mutable ImTextureID  m_logo {nullptr};
    mutable int m_logo_index {0};

    const unsigned int logo_size = 5569;
    const unsigned int logo_data[5572/4] =
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
    0x8a16a08a, 0xa200a928, 0x5eaba98a, 0x9a4a877d, 0x36267170, 0x6800ffa1, 0xc095ddd0, 0xde7345b7, 0xee2ed61f, 0x7c6b0375, 0xf7c8bce5, 0xef0b9547, 
    0xca1f77d8, 0xc5c113ba, 0xaea4644c, 0x8aa22880, 0x288a16a0, 0xa22824a9, 0xf157eaaa, 0x1c3e5959, 0x2286a57d, 0xaecfc951, 0x71653729, 0x7daca896, 
    0x9abb5013, 0xfbd7ecfb, 0xe81e7bbf, 0x7e5f288b, 0xf9e33930, 0x6218c956, 0x77110d3d, 0x45090b57, 0xea105314, 0x02a4a228, 0x5d362b8a, 0x43924c5c, 
    0x246ec2a7, 0xe5288353, 0xfcebe957, 0x489bb4be, 0x45a5b912, 0xdcdc5867, 0xc06ba6c6, 0xb9818065, 0x7dd10094, 0x80d61c40, 0x06a3dc2a, 0xe081d153, 
    0x34b8138a, 0x4c51142d, 0x8aa2e808, 0x47aea0d8, 0xf7aa17c6, 0x3b47f696, 0xc7e4f923, 0x5b00ffbf, 0x595dd7f9, 0x4590e8d0, 0x65a43eab, 0x3c9f4996, 
    0xe9333036, 0x1315a7c7, 0x68c84a4e, 0x621df5e4, 0xd64e6dda, 0x6239caf6, 0x180c0068, 0x1d0f9001, 0x7745a60f, 0xc293ca50, 0x8e72c692, 0x06f581a1, 
    0xf874d4a0, 0x6d433b75, 0x82a46039, 0x7a04f50a, 0x166dac53, 0x643bcec6, 0x1863dd91, 0x7e9cf105, 0x1617a314, 0x288a05ee, 0xb410b4a2, 0x05484551, 
    0xbf243e73, 0x616dd445, 0x1190cc60, 0x5c0f8a24, 0xeb07fd91, 0x8350355d, 0x510d8649, 0x922403f5, 0xb1e1f94a, 0x3d4e9f81, 0x569b262a, 0xf7628e40, 
    0x529fb758, 0x8eb3bcb6, 0x0310da58, 0x0e640006, 0x6bb6c798, 0x581249b4, 0x51ce48d6, 0xc63e30d4, 0x232dd4a2, 0xbc0dadd4, 0x21480ad9, 0x8b22a897, 
    0x6a65513b, 0xba23eb96, 0x33be00a7, 0x4d8b518a, 0x8aa22783, 0x8a16402b, 0x2b24a928, 0x6d99bb9c, 0x99654cbc, 0x728e41fe, 0x1d57d04e, 0x00ff333f, 
    0xa98eaeeb, 0x5bb169db, 0xbd7f4fea, 0x38c995e4, 0xd367606c, 0x26894a8f, 0x27a361ed, 0xaeacd654, 0xd986b6a1, 0x37299a9d, 0x03574097, 0xfcd791d8, 
    0x920ca22b, 0x2867bc2a, 0xc63e30e0, 0x9a42a899, 0x94b7a98d, 0x65081260, 0xd2320dea, 0x245b5ad8, 0x4e38230b, 0x24b47e01, 0x9a6cd4d3, 0x24ab288a, 
    0x52511475, 0x65c67304, 0xcad54882, 0x87a15217, 0xb1d69151, 0xd92f397c, 0x9009dbbc, 0xeb69bea4, 0xe9c781eb, 0x1d4ed55b, 0xd93bde32, 0x7606a42e, 
    0x40569290, 0x15c38f0a, 0x86a63b0d, 0x166a2b99, 0x65ca71f6, 0xcef25764, 0x6ab9d5d8, 0xad854b8f, 0x9b11d8f4, 0x1cb8db71, 0x1fc883e7, 0x6c6ea991, 
    0x68606eed, 0x9650b485, 0x06d038fe, 0x8bdb5a14, 0x4884755b, 0x6ea117ce, 0x0deeecb4, 0x15455193, 0xdeb31b42, 0xfaf49e8d, 0x7661e628, 0x1bbd6733, 
    0x5af5e93d, 0x750bfae2, 0x07f6b624, 0xdf821005, 0x87c66ffd, 0x9bec8236, 0x3d1bbd67, 0x843abcea, 0x395e8072, 0xadfb5621, 0xe85b0128, 0x075ac541, 
    0x9b922223, 0x9edda861, 0xa7f76cf4, 0x85994fd1, 0xf49eddd8, 0xd1a7f76c, 0x76c5cc53, 0x1bbd6733, 0x9e26e93d, 0x93cc782b, 0xcd1d4538, 0x97d73753, 
    0x629f4e28, 0x49d201f2, 0x3be80738, 0x68d449f1, 0x3dbbcb2e, 0x55efd9e8, 0xca5b6de0, 0x6c803ddf, 0x866f441e, 0xfe7ae803, 0x054bbd07, 0x951b57dc, 
    0x7abf0e09, 0xfa651836, 0x6ad8148a, 0x1bbd673f, 0x53f4e93d, 0x267661e6, 0x7b367acf, 0xe6a9a8d3, 0xb31bbb62, 0xf59e8dde, 0x374d5d52, 0x6ec876b4, 
    0x5249461d, 0xa3c68f02, 0xeaa5fa16, 0x1a475c1c, 0x0f3c22f4, 0xfbd49cd4, 0x977ad047, 0xb3d17bf6, 0x6145aade, 0x7e9b46aa, 0x738ecfd0, 0xd3c7b1f2, 
    0xc2e9fca9, 0x378148ee, 0xb20e0a08, 0xaa0f9cc7, 0x3ead1ff5, 0x76961a76, 0xdeb3d17b, 0x19ca1094, 0x828c2048, 0xe6d3d23b, 0x6713bb62, 0xe93d1bbd, 
    0xd4cd0dd5, 0xe40baa56, 0x5e553896, 0x62e7a949, 0xefd924bb, 0x2e91724d, 0x1d5424e9, 0x9f15c732, 0x79dcd435, 0x6f000bd7, 0xb0658ce0, 0xc200ff7d, 
    0x67dd8aa2, 0x666d315f, 0x9196e798, 0xa366fe80, 0x6aea31da, 0xdd558ea1, 0x38e38a1c, 0xb6d39cca, 0xf408d57b, 0xde99f2d5, 0x90e763d9, 0xf0233158, 
    0x7a0a3fce, 0x8686c72b, 0x9bd1b763, 0x8800ff7a, 0x1a24eda7, 0xd17b3697, 0x86a3deb3, 0x37e648e4, 0x32502e5f, 0x7ae49c54, 0xd554dc83, 0x6357ec5c, 
    0xd08aa228, 0xba5b2ba3, 0xb4052196, 0x0f37632c, 0xa83c2083, 0xd5f9e9c8, 0xded22ead, 0x0ea90dde, 0xe482cfe2, 0x4feaf464, 0x58a949e1, 0x8fa9fd2d, 
    0x8cf22281, 0x418b1a7b, 0x9644f596, 0xfb4850f7, 0x07b81b49, 0x1a20a9d3, 0x51a9b58f, 0xe2852135, 0xb242d285, 0x1c538ea3, 0xb5b35d56, 0x9f35afbe, 
    0x394f2722, 0xcb00ff10, 0x7b549e3c, 0x3d8de155, 0x3c6f731e, 0xca050c84, 0x31e03e63, 0x3ccd5a35, 0x65d4e4f9, 0x043f4f5c, 0x0eb42b6d, 0xa7f48331, 
    0xb9d803d5, 0x12551445, 0x5251142d, 0xb9390023, 0x6cfae296, 0x23cc92bf, 0x02179e8c, 0x3b75d2a7, 0x3f0503c9, 0x5711f079, 0xdfa674d8, 0x70634d5a, 
    0x74c3d6f7, 0xffb1cc4d, 0x7ffe3300, 0xaed05d95, 0x7c46c634, 0xec266fc5, 0x19052381, 0xb2d261c7, 0xc7c5de49, 0xdad27273, 0xc3651778, 0xe6038919, 
    0xacf2df53, 0x325b450d, 0x5b938dcd, 0xe4d8e892, 0x43bff260, 0x694f905a, 0xd547ba67, 0xc881caad, 0x72c81f57, 0x61e62a6b, 0x76bc5cad, 0x239845ca, 
    0x198c98cc, 0x7f7ad831, 0xd5e4c69f, 0xf4620fac, 0x49a10952, 0x0c75c040, 0x1d7d1a3f, 0x44150506, 0x7735540b, 0xa1906c71, 0x1048e72b, 0xcf3fecb0, 
    0x5ef5abf5, 0x6d6fd1f6, 0x7a704ccc, 0x0f35f4a9, 0x8eb71861, 0x1967c944, 0x930362f2, 0xfee33fd3, 0x9d06b535, 0xb0a76b6d, 0xcfcad889, 0xa7d763c0, 
    0xea34d6f8, 0xf245feae, 0xf92073f9, 0xff43465d, 0xfabff500, 0xc3732bfd, 0x44d136f6, 0x7138c9d2, 0xc13823bf, 0xc559ebe3, 0xc61d975d, 0x2a2ded8f, 
    0xa77b2059, 0x0b6c8070, 0x56ad47c6, 0xc44be686, 0x11636996, 0x872124ca, 0x9f9ee74c, 0x67436e8d, 0x25191363, 0x39829b75, 0x4bf5b2e2, 0x88e116d8, 
    0x777f3250, 0xfc966304, 0x5bbb4a07, 0x15b1e5a8, 0x27eeee8f, 0xd7f24881, 0x9d83a26b, 0xf38f1cb9, 0xb56a57eb, 0x1ec4bb9d, 0x4ce7b159, 0x3de8f872, 
    0x6956e107, 0xacc58cad, 0xf76d6459, 0xa3cb4d57, 0x91fdd398, 0x00fffad3, 0x65d5d69c, 0xb3d6405e, 0xf8263a49, 0x19bd1f25, 0xb5fe7be4, 0x2e6d0212, 
    0x59b734d6, 0x9c2ccdcf, 0x74453d96, 0x8b4edb16, 0x97641870, 0x3170fdc2, 0xe202a45c, 0xca5977dd, 0xdd4fc6b2, 0xa5add56e, 0xdefeae7d, 0x2fd29945, 
    0xed18219f, 0x11cd8103, 0x77542c7d, 0x3d2d3435, 0x77ccc5ad, 0x2333b78c, 0x1ecf0870, 0x557faed9, 0x0defd345, 0x33e549cc, 0x9cc377c8, 0xadf43a02, 
    0x3a41371d, 0xd531cd64, 0x1f974fa2, 0x56356323, 0x509b34e2, 0xdc2b836b, 0xddddc630, 0x6b0fbd07, 0x82d87258, 0x795184f1, 0x21a165c0, 0x23999ed7, 
    0x68cdf123, 0xdb7e7e56, 0x1a1e952f, 0x7e656015, 0x153847cc, 0x652042a1, 0x59322515, 0x230384a3, 0xee009e01, 0xe7b4827e, 0x5645890b, 0x025cf692, 
    0x1f2f9188, 0xe0afc734, 0x34dcb32a, 0x8e7f0f83, 0xa762807f, 0xcd05f9da, 0x0c842a2a, 0x5277f9fc, 0xe61f7fbe, 0xdc12590d, 0x7f6d3726, 0xcf9f3662, 
    0xfb29fda1, 0x35b9204f, 0x92704514, 0xe02aa39d, 0x1a0ca364, 0x0bb49f92, 0x24a4a28c, 0x00902428, 0x6b564dea, 0x8098cbcd, 0x3d00ffdb, 0x00ff811f, 
    0x82f6a9d7, 0x1c472db9, 0xaafec310, 0x54f74f14, 0x49beaf0a, 0xa03483b1, 0xc546f783, 0xe8c21335, 0xffc7e33f, 0xfcdd1e00, 0x90af7daa, 0xab3268ee, 
    0xca5056a9, 0x20230846, 0x407cd6d2, 0x6e96f90c, 0x8f84303d, 0x8942a6d6, 0xc4c2988f, 0xfd95dcfd, 0x5f7b140f, 0x8a6ab920, 0x7095268d, 0xfdc17c1d, 
    0x973f18e5, 0xa9f2c37f, 0x0e44d6d1, 0x083d1584, 0xb882f6aa, 0xb82429da, 0x462d7f54, 0xa7effee7, 0x92eeabd6, 0xc765e0e0, 0x41edb443, 0xf6a8f7f8, 
    0x8b4b7281, 0x64936e68, 0x9e0e1cc8, 0x5d3a04d5, 0x69ec04bc, 0xc23ce879, 0x2ac6ed31, 0x1c97b565, 0x84fa9148, 0x2169534a, 0xf7df72e9, 0x207f2ecd, 
    0x1167d3b8, 0xfd37dd18, 0xa4f17ffe, 0xb5b6b1b7, 0x15c4d062, 0x7f927c8f, 0x94b5785a, 0xdf7dcb7f, 0xeb00ff55, 0x8c73f9d2, 0x3f8651e5, 0xe74f9084, 
    0x5c903f47, 0x46858a92, 0x1393c2d6, 0x01c6e19f, 0xc21f3afc, 0x0bb949a6, 0x849e1426, 0xf4f90f1e, 0xe202eda7, 0xaf3451d1, 0x19b9da21, 0xf1d78be8, 
    0x6ecba93d, 0x47db8271, 0x6afc53f7, 0xae90a77d, 0x494b9f54, 0x7360f325, 0x38daf703, 0xdddab034, 0xd124012a, 0x84fb3812, 0x7815b9fe, 0x00fe1041, 
    0x2fcdeb73, 0xf3fc1793, 0x52e4fb4f, 0x620ef2e6, 0x7f98b7a0, 0x18bf48cb, 0x1500ff8f, 0xc852364d, 0x78c93d71, 0x9ae700ff, 0x8ef9c12e, 0x89a071d5, 
    0xfaa83187, 0x4d604871, 0xd18705b8, 0x206f3e8d, 0x515111e6, 0x5185a242, 0x484b01d0, 0x1807b8d1, 0x3f78e871, 0x23464de1, 0x37aa1fee, 0x54fef06f, 
    0xb8204f7b, 0x788b3397, 0xe871e34b, 0x386beaab, 0x2270af24, 0x1b390f57, 0x60e7469e, 0xfdf103f4, 0x08a4de6a, 0x44c62481, 0x7a6c0672, 0x58657855, 
    0x23670635, 0xb736ca64, 0x2ce31e6a, 0x1fc1a7c5, 0x3dd73c2e, 0x6dcdf959, 0x973eba69, 0xb72c6373, 0x3142a856, 0x007065e4, 0xd667b507, 0xe9e49eae, 
    0xbc9c6eed, 0xc0e9f4b2, 0x22693503, 0x8e15b9a3, 0x56cde18b, 0x09d29236, 0xeefb0300, 0x4d9ed731, 0xe6695c64, 0xb62359de, 0x11a39499, 0xb1953b86, 
    0x1fcdadf5, 0x9abb4753, 0x6c414bca, 0x644566b2, 0x67dc977f, 0x54471503, 0xe42eac48, 0x39082043, 0x2b0d8d05, 0xe6da725c, 0xba723e50, 0x6dbc3138, 
    0xbef2b483, 0x8b557b84, 0x2a445a69, 0x337e00ff, 0xbfaf8fb5, 0x3e6155e3, 0x8d5cf374, 0xa0c224c1, 0xe23bf6fe, 0xf653b4a6, 0x843f898b, 0x51f55195, 
    0x23885283, 0xb83b98ba, 0x5117c53c, 0xa9f4e6fd, 0xf3c8b460, 0x3cf7062e, 0xf0173d85, 0x4f0877aa, 0x25fe23da, 0x508fc990, 0x1629526b, 0x3944d658, 
    0xc46aa506, 0x7bb4b63a, 0x298b3b1b, 0x768fba66, 0x23ee3cc6, 0xacf60003, 0x44d2db5a, 0x1c49a68f, 0x7cd39d60, 0xeac098de, 0x43e5d66a, 0x6d1b5b72, 
    0x9a895617, 0xef1837d2, 0xfaf50796, 0x2ac41556, 0xc3094993, 0x475eb1aa, 0xf4b60607, 0x77ed435d, 0x61df1697, 0x78e3e836, 0xa1af9c31, 0xcb9a03e7, 
    0xd296add4, 0x895148fe, 0x5ecfc851, 0xc755bd69, 0xcbe85a2b, 0xbe6e239a, 0x63f74364, 0x3dea4f97, 0x38e5df28, 0xc4285b91, 0xe8c1f407, 0x55c9fbd4, 
    0x2d4fd92d, 0x3aaa2a20, 0x844e5093, 0x8d242c5a, 0xdc636cc7, 0x816c56e4, 0x73cd8762, 0x39516219, 0xafdadd73, 0x0c000000, 0xab0ad001, 0x2ccb7668, 
    0xc36da967, 0x48aa55dc, 0x25ba5b41, 0x84c58d85, 0xbbc5ddb3, 0x883be363, 0x35f4c0c0, 0x20da5b85, 0x2ad8d376, 0x24711e17, 0xd53ae876, 0xaee072c7, 
    0xb42d6cc9, 0x3e395e2d, 0x4ea91bc7, 0x3f02cb09, 0xc7135833, 0x1049dce4, 0x5c39b63b, 0xf6d6e0fa, 0xfb0ba98b, 0x3d5da0f9, 0xf26863ed, 0xf1044f59, 
    0x598301dc, 0x59aaa51a, 0x8cc212ea, 0xcf08824a, 0xeaa1795e, 0x6b2539ae, 0xc8a29ba2, 0x1e6458a5, 0x0b665bd5, 0x31ce2741, 0xffba2795, 0x55ab8800, 
    0x92ebbe03, 0x09424407, 0x4abf3df7, 0x144b6686, 0xe4c02542, 0xa4d6a79e, 0x222307a4, 0xb7829096, 0x3e0d3bb4, 0x7b964fe3, 0xc8b2b7c8, 0x3888f940, 
    0x8335f4c0, 0x0e87075d, 0x7023df2d, 0x678c1312, 0x1d55edb0, 0x25bb82cb, 0x0db4edd3, 0x484b6465, 0x02d08d65, 0x03041677, 0xae48f2d0, 0x222fe27a, 
    0x6e4358e6, 0x7daedcf2, 0x0e5d7170, 0xaac3ab95, 0x76ac2d5f, 0x49a16c0f, 0xfe00872e, 0xb5b16240, 0xd4b2446b, 0x2c3682a5, 0xd73382a0, 0x6b6f9a91, 
    0xe85a498e, 0x1e4b51a7, 0x7578bde1, 0x52edefe9, 0x520533d2, 0x3519a0c7, 0x0c086306, 0xf0202308, 0x377b326b, 0xe7a91080, 0x7f3be75b, 0xa3953e89, 
    0x3e6d9cbf, 0x553dbdc3, 0xfc18de8f, 0xcaa2dc6a, 0x200889b9, 0xa8ecd662, 0xb41af9c8, 0xbd9bdbb4, 0x5049b138, 0x65961cbb, 0x3ec5f424, 0x66de3e5d, 
    0xde8df1de, 0x52fa83a3, 0x3f1c63c3, 0xdebf9d75, 0x494a7f62, 0x162b35a4, 0x2e2fd666, 0xa3d22454, 0x78c4c801, 0xf0d69a35, 0xa54a3306, 0x26999198, 
    0xa8d6b86a, 0x1f9c25e7, 0xe48f5cf6, 0xdba7336a, 0xbdc90e97, 0xbf257787, 0x98e4a699, 0xaeae5c39, 0x3bf747f7, 0x31bd1f82, 0x7f14fc18, 0x6b1cb75a, 
    0x42116114, 0xe214f4aa, 0x470e7f18, 0x713ca3b1, 0xc81cadcf, 0x16374457, 0x46b242e6, 0x515f64c1, 0x54430fc3, 0x528a66e3, 0xf2256f23, 0xd3fd1373, 
    0x5245adf4, 0x6c0a8c7a, 0xbbccd1b6, 0x9ede5064, 0x2bb4a5a2, 0x2edb97a2, 0xb86d8357, 0x2b9eaae3, 0x2cf14847, 0x5f3491f6, 0x8b19dc65, 0x7e9c0016, 
    0xbaa64a5f, 0xcbc7c673, 0x2b7b21b9, 0x4dad7f2e, 0xf928b31d, 0x427d0ce4, 0x53d2fae3, 0x58a951b7, 0x97c597d3, 0xf696ac8e, 0xe5707c98, 0x2b523f0e, 
    0x27d4a511, 0x18cdf362, 0x11333f76, 0x0a00ff96, 0x8fe4d6b2, 0xed3fc095, 0x30ad7f02, 0x8971cada, 0xb6f360c2, 0x26f1a33e, 0x60f79d9b, 0x31c87de5, 
    0x471e2c1c, 0x66c04326, 0x15b08fe5, 0x7116312c, 0x81b48b2c, 0x1ee75c85, 0x965a00ff, 0x24e1482b, 0x5d3d968c, 0x549e26b9, 0x517bdc8e, 0x507485cc, 
    0x94851692, 0xbb5f4012, 0x6aeced28, 0x0028bc55, 0x8e9e319d, 0x0c5783bc, 0x200f8260, 0x735504f5, 0x19e61460, 0x19d3634c, 0x6cc91c15, 0xdb3ea12b, 
    0x3d00ff6f, 0xfa5a433f, 0x30f6b866, 0x72a47034, 0x93ec362b, 0xc94a7ff8, 0x6e1c02fb, 0x5cf448da, 0xb1705a7f, 0xff0a7f88, 0xd434df00, 0x6c2b35ec, 
    0x2e8abf6f, 0xc4b090ca, 0x5c93d7a7, 0xcf92baf4, 0x4c323933, 0x5e4872ec, 0xb100ffb4, 0xffd4fdc7, 0x3c05be00, 0xf88d8143, 0x1403a003, 0x07f54bdd, 
    0x2f57ee2b, 0x3c80ab34, 0x589e7b90, 0x00a91dfe, 0x868722c6, 0xf7ed7aee, 0x5a33f53e, 0xddfa37b3, 0x18d5fda9, 0x12a9f127, 0xa6428d15, 0x73a91d00, 
    0x61155d21, 0x39b8c494, 0xe538e864, 0x2465b57e, 0x81510649, 0xae6a8f14, 0xbd550277, 0x2bd93045, 0x7af3cd72, 0x649e8a81, 0xa5d54417, 0x69736ba6, 
    0xc4513490, 0x65b7198e, 0xff58e9b3, 0xd2616200, 0x04a9c0e1, 0x358f8152, 0xfa3fd54f, 0x05d2d4f4, 0xbf6f6c2b, 0x48192f8b, 0xeb536258, 0x6549acc9, 
    0x6569e579, 0x24c7ce62, 0x2ecff49e, 0xbfe8f95f, 0xe9f57f82, 0x72ae688d, 0x9ee9cfec, 0x523795df, 0xe0be72e3, 0x1ba7c6d3, 0x93177db2, 0xe6649251, 
    0x21071540, 0x3875fe49, 0x90c22880, 0xa418a007, 0xef727e5b, 0x0063d0bb, 0x5d21f354, 0x30a8880a, 0xd3fad407, 0x8298a2a8, 0x57e9d9ad, 0xb634f1b7, 
    0x3845efd0, 0x1f8c7a27, 0xe94ad5c4, 0xe35a207c, 0xb01bb94f, 0xf003c704, 0xb1bbd214, 0x32765770, 0xacfac3df, 0x12da5c68, 0x75e567a0, 0x386be46f, 
    0xed1a1c8c, 0xfd3bebf4, 0xb8a4193a, 0x032737bb, 0x736f0f6a, 0x3be3755c, 0x44f24cde, 0xc5726762, 0xe7e4f00f, 0x39ace414, 0x4d432445, 0x46cf0d74, 
    0x8a3a45ec, 0xa2683193, 0xb90a908a, 0x5fdfa467, 0x43dbd2c4, 0xd2b622e6, 0xf1e7a877, 0x2a464235, 0xd8b58e84, 0x69372978, 0x65ac9e33, 0x45f79f3c, 
    0x8add5554, 0x999d7984, 0xd51efe86, 0x361b4b51, 0x94757f20, 0xce1ac89f, 0xad070723, 0x6599067a, 0xdf14657f, 0x9937ba6d, 0x8f3a88fb, 0x5dc235a1, 
    0x5ca88db4, 0xdf98b1f9, 0xf55062cd, 0x9ce29c5c, 0x29ce61a3, 0xa2282a22, 0xa26881a0, 0x5c05918a, 0x0d35d2b1, 0xce96264a, 0x6d45cddf, 0xcf51efa4, 
    0xe5816ae2, 0x6f57d8b1, 0x934e82e0, 0xf3c1ab70, 0x91ef3f8e, 0x62771555, 0x66671ee1, 0x6f78d273, 0xbd0b895a, 0x761d6d83, 0x81fcb1ba, 0xa45766cd, 
    0x957a5669, 0xf5355384, 0x0224bde1, 0xc48f3a88, 0xea9ed7fa, 0xb400ff05, 0x8c481eae, 0x68b1f22e, 0xe7e4f0cf, 0x211be514, 0x1a2229ce, 0xcca0a228, 
    0xa4a2285a, 0x34ec5741, 0x8554473d, 0xf3b7b3a5, 0x3b695b51, 0x137f70d5, 0xf0bc2b54, 0xb9472720, 0xce99e000, 0x5504fc0f, 0xb8d85d45, 0x9cd94945, 
    0xd616becc, 0x60ef42e1, 0xba761dd8, 0xac81fcb1, 0x23380892, 0xe9a75704, 0x9f9e7a36, 0x7a7ded14, 0xa200496f, 0xe71ea30e, 0x51e7bcd6, 0xe54a1d32, 
    0xedc288a5, 0x8f8c312b, 0x539c93bb, 0x38876c94, 0xa895ab28, 0x31838aa2, 0x95745594, 0x7e5c8e97, 0x4613a4a2, 0xdf078b7f, 0x12e5d68a, 0x5fe14b57, 
    0xf7393b2a, 0xa55b14cf, 0x3fd80c38, 0x64ae7674, 0x60ddb732, 0xa996867e, 0x19953b3b, 0x569dbb72, 0x695ba69d, 0xaccf3432, 0x82112245, 0x2c3f0aa3, 
    0xb116d69c, 0xa7f61d77, 0x460cf12c, 0xa8231770, 0x54aa1903, 0xf34175d6, 0xd2db1430, 0xb2ba72c3, 0xe747451d, 0xcd8afb29, 0x5b531cf4, 0x02ec322c, 
    0xe2c703fa, 0xa227486a, 0xc653249a, 0x3a0d7d70, 0x0c6504a4, 0xd5f51ea4, 0xe0583a78, 0x96232ed3, 0x9cd24d54, 0x843f606e, 0xd472cd73, 0xe3ce4e53, 
    0xce5db98c, 0xecd34ac3, 0xe59e39f4, 0x05e458b5, 0x5170b771, 0xb9e664f9, 0x3bee62cd, 0x886b52dd, 0x60c4d881, 0x1903a79e, 0x6e5354aa, 0xba72c3ea, 
    0xce280ab2, 0x8f4bd33a, 0x95bef9e1, 0x8aa28f24, 0x9735442a, 0x975debb8, 0xe36d2e83, 0x6912aed2, 0xc78c8de3, 0xe10f981b, 0x4ed1c715, 0x46e5ce2e, 
    0x6de7ae5c, 0x7eda69a4, 0x3edd3385, 0xb4ab1cb5, 0x5f85a08d, 0xcb3527cb, 0xead177eb, 0x50d7c47a, 0x05242282, 0x1838c1c8, 0x9ba251cd, 0xe586d595, 
    0x51146475, 0xd1024945, 0x0a221545, 0x7113fced, 0x7291166f, 0x19475c93, 0xb8038e69, 0xe28aee07, 0xb38baa28, 0x2b9751b9, 0xda68deb9, 0x4ea09f66, 
    0x716b52f7, 0xb4910aca, 0x19f1a310, 0x5f53ae39, 0xd6528bbf, 0x1184ba67, 0x9114101b, 0xcd184082, 0x95435167, 0xaedc90d5, 0x455190ac, 0xff1f2415, 
    0x000000d9, 
};
};
} // namespace BluePrint
