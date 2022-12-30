#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <Radial_vulkan.h>

namespace BluePrint
{
struct RadialFusionNode final : Node
{
    BP_NODE_WITH_NAME(RadialFusionNode, "Radial Transform", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Fusion#Video#Move")
    RadialFusionNode(BP* blueprint): Node(blueprint) { m_Name = "Radial Transform"; }

    ~RadialFusionNode()
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
                m_fusion = new ImGui::Radial_vulkan(gpu);
            }
            if (!m_fusion)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_first.type : m_mat_data_type;
            m_NodeTimeMs = m_fusion->transition(mat_first, mat_second, im_RGB, progress, m_smoothness);
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
        float _smoothness = m_smoothness;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        ImGui::Dummy(ImVec2(200, 8));
        ImGui::PushItemWidth(200);
        ImGui::SliderFloat("Smoothness##Radial", &_smoothness, 0.0, 1.f, "%.1f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_smoothness##Radial")) { _smoothness = 1.f; changed = true; }
        ImGui::PopItemWidth();
        if (_smoothness != m_smoothness) { m_smoothness = _smoothness; changed = true; }
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
        if (value.contains("smoothness"))
        {
            auto& val = value["smoothness"];
            if (val.is_number()) 
                m_smoothness = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["smoothness"] = imgui_json::number(m_smoothness);
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
        // if show icon then we using u8"\ue918"
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
    float m_smoothness   {1.f};
    ImGui::Radial_vulkan * m_fusion   {nullptr};
    mutable ImTextureID  m_logo {nullptr};
    mutable int m_logo_index {0};

    const unsigned int logo_size = 4483;
    const unsigned int logo_data[4484/4] =
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
    0x280aa08a, 0x280a80a2, 0xf1dd1aaa, 0xa7a9b4d3, 0x3626db46, 0x3dda7fc6, 0x7693c63f, 0x15fd0257, 0x5b7bf8cb, 0x7ed4b8be, 0xbb49a8cb, 0x79748fcd, 
    0x87fdbe50, 0xa8abfc71, 0x2b2919a5, 0xa2280aa0, 0x288a02a8, 0x2a8a02a0, 0x747cb786, 0xd1692aed, 0xb18dc9b6, 0x4f8ff69f, 0x95dda4f1, 0x7245bfc0, 
    0xefd61efe, 0xb21f35ae, 0xf36e12ea, 0x541edd63, 0xdc61bf2f, 0x29ea2a7f, 0xe84a4a46, 0xaa288a02, 0x288aa200, 0xa18aa200, 0x3b1ddfad, 0x6d749a4a, 
    0x676c63b2, 0xfcd3a3fd, 0x70653769, 0xbf5cd12f, 0xebbbb587, 0xbaec478d, 0xd8bc9b84, 0x0b9547f7, 0x1f77d8ef, 0x518abaca, 0x00ba9292, 0x802a8aa2, 
    0x008aa228, 0x85f1912b, 0xbde5bdea, 0xfec8ce91, 0xffef3179, 0x75fed600, 0x3a7456d7, 0xcf6a1124, 0x926519a9, 0x8c0dcf67, 0xe971fa0c, 0x92d344c5, 
    0x3d391ab2, 0x9bb65847, 0xb2bdb553, 0x009a588e, 0x64000603, 0xa60f9d0e, 0xca507745, 0xc692c293, 0x81a18e72, 0xa30635f6, 0xdaa9c3a7, 0x05cb691b, 
    0xa8571024, 0x639dd223, 0x7136b668, 0xeb8e24db, 0x8c2fc018, 0x18a5f0e3, 0x2c70b7b8, 0xa0154551, 0xfdc5f689, 0x8890d6df, 0xb5fef087, 0xcd4d6319, 
    0xadbea960, 0x92fbca91, 0xfd641f13, 0x86f1916b, 0x0e4d0ba5, 0xa852609e, 0x6df98e44, 0x38873dc3, 0xbda39ee9, 0x0e9d5575, 0xb35a0489, 0x645946ea, 
    0x63c3f399, 0x7a9c3e03, 0x3b285571, 0x8e951659, 0x2dd6514f, 0x6fedd4a6, 0x2696a36c, 0x80c10080, 0x43a70319, 0xd45d91e9, 0xa4f0a432, 0xa8a39cb1, 
    0x418d7d60, 0xeaf0e9a8, 0x72da8676, 0x150449c1, 0xa7f408ea, 0x8d2dda58, 0x23c9769c, 0x0b30c6ba, 0x29fc38e3, 0xdc2d2ec6, 0xa2288a65, 0xec7711b4, 
    0xf5777ff1, 0x5f449e34, 0x294dfddd, 0xafa48d7a, 0xfdd53e2a, 0x67ae73a6, 0x118a71d4, 0xaea93ffc, 0xc65ac573, 0xf69f158f, 0x5c5ada74, 0x26dc96dc, 
    0x501e6469, 0x60051c23, 0x35d7eb09, 0x654deeb8, 0x16c1a245, 0x9691faac, 0xf07c2659, 0xd36760c4, 0x8dea4a8f, 0x6d56db29, 0xe4acd604, 0xda621df5, 
    0xf6d64e6d, 0x686239ca, 0x01180c00, 0x3e743a90, 0x43dd1599, 0x4b0a4f2a, 0x863aca19, 0x1ad4d807, 0xa70e9f8e, 0x2ca76d68, 0x5e419014, 0x754a8fa0, 
    0xd9d8a28d, 0x3b926cc7, 0xbe0063ac, 0xb7c28f33, 0xb95b5c8c, 0xa2288a65, 0xec7711b4, 0xf5777ff1, 0x2f9e7d34, 0xa4a6feee, 0x6b5fbca2, 0xc999f953, 
    0xfd91fbcc, 0xfeee2f9e, 0xb2bba6a6, 0x6278cbf0, 0xd356e3e2, 0x1471d9be, 0x3a5fc987, 0x1ce50344, 0xa96758e1, 0x50b1d9a8, 0x9b599823, 0xc149b225, 
    0x5b3132e8, 0xd593e753, 0xfa6ed39a, 0x6aa9759e, 0x76ead2f6, 0xc951b6b7, 0x60004013, 0x78800cc0, 0x45a60fc6, 0x93ca5077, 0x72c692c2, 0xf681a18e, 
    0x61a70e35, 0xd66aa90e, 0x480aeef3, 0x4750af20, 0xa28d2da5, 0x6cc7d9d8, 0x63ac3b92, 0x8f33be00, 0x4508bdc2, 0x586eeea6, 0x412b8aa2, 0x40511405, 
    0x26531405, 0x5e783b9a, 0x716c5b69, 0x823e66a9, 0x53451f80, 0x75acd5b1, 0x9fb47516, 0xc33228cc, 0xc5fc1869, 0x7b9aa45c, 0x14455100, 0x8aa228c0, 
    0x99a22800, 0xc2dbd134, 0x63db4af3, 0xf4314b8d, 0x2afa0014, 0x63ad8e9d, 0xa4adb3a8, 0x964161fe, 0xe6c7481b, 0xd324e52a, 0x288a02d8, 0x528604a6, 
    0x664b537b, 0x844a459b, 0x0ab2c256, 0x344da628, 0xd2bcf076, 0x52e3d8b6, 0x50057dcc, 0xec54d1c7, 0x451d6b75, 0xf3276d9d, 0xdab00c0a, 0x57313f46, 
    0xc09e2629, 0x30455114, 0xfdaf7d2c, 0x7583d68f, 0x55ebc7fe, 0xaf3eace8, 0x911cb14b, 0xf8e73326, 0xbe8c5a7f, 0x9369537b, 0x2fbc1d4d, 0x38b6ad34, 
    0x411fb3d4, 0x3bc2a956, 0x1f529422, 0xd5b15345, 0x751675ac, 0x28cc9fb4, 0x1869c332, 0xab5cc5fc, 0x14863d4d, 0x53708a51, 0xef006943, 0xdffdcbb3, 
    0x79f651d4, 0x8afabb7f, 0xbf7945bb, 0x73649f5b, 0xfb5256fb, 0xfdddbf3c, 0xf8a94545, 0xf69ac457, 0xe9a2e98e, 0x0d6900ff, 0xce1bc926, 0xcf81368d, 
    0xadf630f1, 0x1d3ced2a, 0x255a0473, 0x47dc72d4, 0x1ba73413, 0x47f70398, 0x7226d634, 0xb60997d1, 0x8e09cfec, 0xe231fcd1, 0x0ce5684b, 0x93042aab, 
    0x1f3ca123, 0x8d5d81c8, 0x8297f152, 0x70d7488c, 0xd0ed866a, 0xa7a3a8fc, 0xd34f93d3, 0x3e7d9665, 0x25199d17, 0xa40c87db, 0x060f8e1c, 0xd13de9ba, 
    0x288a6ab1, 0xf649c4ad, 0xfabb7f79, 0x2fcf3e8a, 0x57517ff7, 0xeb33af68, 0x7f8eec73, 0x675fca6a, 0xa8bffb97, 0x0a3fb5a8, 0xd15e93f8, 0x3b5d34dd, 
    0xd9a421ed, 0x4da3f321, 0x4cfc73a0, 0xbb4aab3d, 0x5bdc047f, 0xa45ca4c5, 0x5ac611d7, 0x01ee8063, 0xc49aa2fb, 0xe1325ace, 0xe199dd36, 0x9e376992, 
    0xbc35f10e, 0x10499017, 0x58480291, 0xc1c19371, 0x6b808ce9, 0xebd6aeab, 0xc132dac1, 0xb47a9724, 0x15521837, 0xfa3d5078, 0xb8946b9e, 0x3056da48, 
    0x32701082, 0xad779c31, 0x474b55e9, 0x7237a7a1, 0x84e214ad, 0x52d76052, 0xffb86392, 0xfdcbb300, 0xf651d4df, 0xfabb7f79, 0x7945bb8a, 0x649f5bbf, 
    0x5256fb73, 0xddbf3cfb, 0xa94545fd, 0x9ac457f8, 0xa2e98ef6, 0x0d69dfe9, 0x9d0fc926, 0x9f036d1a, 0x5aed61e2, 0x26f8db55, 0x222ddee2, 0x8eb826e5, 
    0x071cd332, 0x14dd0f70, 0xd17226d6, 0xecb60997, 0xc08f1ecf, 0xc6f022fe, 0x69d26ebb, 0x180fb473, 0x325c6459, 0x0ff27a82, 0x3c6e0dae, 0x1de54652, 
    0x08eab00a, 0xe9a457e4, 0xa19b561a, 0xbacc254d, 0x422953d4, 0x7f1532a5, 0xe49a9311, 0x7dabebb5, 0xe29e5743, 0xc4c436dd, 0xc019e305, 0x88a36ac6, 
    0xe5a25ae4, 0x07b32b3b, 0x69a3f469, 0x4c2186ab, 0x12abd631, 0x87d03e99, 0x00fff2ec, 0x7d14f577, 0xfeee5f9e, 0x5cd1aea2, 0x9f5b00ff, 0x59ed6764, 
    0xfff2ec4b, 0x15f57700, 0x5fe1a716, 0x3bda6b12, 0x7da78ba6, 0x249b34a4, 0xb469743e, 0x87897f0e, 0x6f5769b5, 0x788b9be0, 0x9a948bb4, 0x4ccb38e2, 
    0x3fc01d70, 0x99585374, 0x265c46cb, 0xdf3cb3db, 0xe24bf846, 0x8e53375d, 0xc31ac5e8, 0x7c9105e5, 0x394e2cd5, 0x471d0703, 0xc69a5bad, 0x299e795b, 
    0xe0d09191, 0x7a450eab, 0x69a68d16, 0x75ef04fa, 0xa01cb726, 0x0a411ba9, 0x9a93113f, 0xf8fb35e5, 0x7b662db5, 0xb11141a8, 0x24184901, 0x75d28c01, 
    0x83a615a5, 0x38469a9c, 0x55be1487, 0xeb64454d, 0x30f39849, 0xaea8b3a2, 0x00ffa99f, 0xf634f07b, 0x218d665e, 0xfa28faac, 0x00bff79f, 0xa7655ef6, 
    0x6919a819, 0xba66a094, 0xa9934f23, 0x8a12366a, 0x09a45295, 0x8d28ab44, 0x2b8ac5ca, 0xfae78a3a, 0x03bff79f, 0x68e6653f, 0x0ed459d1, 0xfb4f7db4, 
    0x2f7b80df, 0xc29a4633, 0x4ecda0aa, 0x974a58ab, 0x6c1c5227, 0x29a39235, 0xa1a58056, 0x3b057157, 0x459d1505, 0xef3ff56f, 0x2ffb057e, 0xa28a4633, 
    0xcda153ad, 0x8f3b3c63, 0x4f2c5c52, 0xaea52045, 0xc5a43063, 0x5100142d, 0x8ad40355, 0x34a8bcf5, 0xe889855b, 0x56a7a9a6, 0xa280582d, 0x5100298a, 
    0x9d011445, 0x3cfb5d45, 0x4dfddd5f, 0xfb8b671f, 0x4fafa9bf, 0xceec70eb, 0x558a6a8f, 0x3abe5b43, 0xe8349576, 0xd8c664db, 0xa747fbcf, 0xd9e7d6f8, 
    0xeaeffee2, 0x7cd0a36b, 0x11efe119, 0xafbe12e9, 0xc3a4fda7, 0xf9fbd931, 0xfe281f13, 0x47530feb, 0x102da3d6, 0x632735e3, 0x2e17bccd, 0x6feaadb1, 
    0x551a726b, 0xd8568631, 0x838e3b06, 0x4b9aaeb8, 0x44f20443, 0x9f8a51dd, 0xe17bd7c0, 0xec3afd7b, 0xa2679fb6, 0x166b8cad, 0xe0d9c437, 0x036c0170, 
    0x7db9c69f, 0x373dc27a, 0x27d69e58, 0x1b418a2c, 0xcec8488e, 0xb4ce730d, 0xe51474b9, 0x2396d1a5, 0x2d6d3bc5, 0xd9e4cc15, 0x2aeacc85, 0xfee2d9ef, 
    0xfb68eaef, 0xfddd5f3c, 0x5b3f7a4d, 0xed6f6687, 0xeba84a51, 0xfad9df17, 0xc386936c, 0x00ffd4fd, 0x9c00ff78, 0xe2d9dfd6, 0x6beaeffe, 0x197cd0a3, 
    0xe911efe1, 0xa7afbe12, 0x31c3a4fd, 0x13f9fbd9, 0xebfe281f, 0xd647530f, 0xe3102da3, 0x33632735, 0x78421ac0, 0xbc46d286, 0x4e55592e, 0x3d236dec, 
    0x0b6aec08, 0x796b71ab, 0xc26db83d, 0x7a371929, 0xdd35e3e0, 0xb65a7778, 0x20ecd9d4, 0x10c7bed2, 0xdf42ef8f, 0x90311d28, 0xd7912b06, 0x75d323ac, 
    0x72a2ed89, 0xb811a4c8, 0xf38c8ce4, 0x321f555c, 0xd22c15b9, 0x8aa2ce68, 0x3a13132b, 0x78f6bb8a, 0x9afabbbf, 0xf717cf3e, 0x9f5e537f, 0x9dd9e1d6, 
    0xab14d51e, 0x4f934953, 0x01f512de, 0x96a96067, 0x0dfd2423, 0xbf78f641, 0xe99afabb, 0x1de139bc, 0x5ed35c0b, 0x69b252e3, 0x319fb6a4, 0x6524cf32, 
    0x857f5048, 0xc44bad87, 0x0ec96a42, 0x35765232, 0xc327ad34, 0x8bbcdd3a, 0x24a90469, 0xcc6d6e60, 0x91d4cf08, 0xb8fbb8da, 0xf6e4b505, 0xa408b7e1, 
    0x83ebdd64, 0xa077d78c, 0x12ebd0eb, 0x981e69cb, 0x25a1506d, 0x303f6ee2, 0xfa710d06, 0x6e7a84f5, 0x4eb43db1, 0x37821459, 0x9e91911c, 0x5a698e6b, 
    0xcaa589e8, 0xa2a8339a, 0xcfc4c88a, 0x450ea5c9, 0x5f3cfb5b, 0x1f4dfddd, 0xbffb8b67, 0xe245afa9, 0xdf8cbea9, 0x268144da, 0xbcec5ac7, 0x3a69a621, 
    0x7bed8ca5, 0x93549608, 0x9d32de8f, 0x1e811db8, 0x9e7dcaf5, 0xa6feee2f, 0x6807efba, 0xa3d5755a, 0xbe14cd5f, 0x8df9b47d, 0x3e92b8a3, 0x56080e8a, 
    0x9a638f00, 0xde94cac9, 0x29e32288, 0x2bfdb417, 0x4ae2fac2, 0xc6f256b6, 0xbb24e3d1, 0xa9d33302, 0xb771b523, 0xc96b0b70, 0x116ec3ed, 0xd7bbc948, 
    0xefae1907, 0xd6a1d741, 0x3dd29625, 0x42a9da30, 0x7edcc44b, 0xe31a0c60, 0xf408ebf5, 0x687b62dd, 0x0429b29c, 0x2323396e, 0x4a13d63c, 0x5c9a42d7, 
    0x8a3aa3a9, 0x428cac28, 0x02a0288a, 0x125fb4b6, 0xcfb668dc, 0x20473004, 0x934bde77, 0x4a7f809e, 0x6d9aa2c5, 0xd5da346c, 0x3c9e441d, 0x4b0819d4, 
    0x468f1168, 0xaef5e3e4, 0xb9e5796e, 0xcb99e79d, 0xc72ce7c8, 0xa1e8a8b9, 0xe4c6bdc9, 0x1445e1de, 0xa2282452, 0x6b2b008a, 0xc62df145, 0x43f06c8b, 
    0x7d077204, 0xe839b9e4, 0x5aacf407, 0xc3d6a629, 0xd451ad4d, 0x41cde349, 0x81b68490, 0x4e6ef418, 0xe7e65a3f, 0xde995b9e, 0x8ebc9c79, 0x9a7bcc72, 
    0x9b1c8a8e, 0xee4d6edc, 0x22455114, 0xa0288a42, 0x5fb4b602, 0xb668dc12, 0x473004cf, 0x4bde7720, 0x7f809e93, 0x9aa2c54a, 0xda346c6d, 0x9e441dd5, 
    0x0819d43c, 0x8f11684b, 0xf5e3e446, 0xe5796eae, 0x99e79db9, 0x2ce7c8cb, 0xe8a8b9c7, 0xc6bdc9a1, 0x45e1dee4, 0x28245214, 0x2b008aa2, 0x2df1456b, 
    0xf06c8bc6, 0x07720443, 0x39b9e47d, 0xacf407e8, 0xd6a6295a, 0x51ad4dc3, 0xcde349d4, 0xb6849041, 0x6ef41881, 0xe65a3f4e, 0x995b9ee7, 0xbc9c79de, 
    0x7bcc728e, 0x1c8a8e9a, 0x4d6edc9b, 0x455114ee, 0xa7064622, 0x976c5003, 0x28adbffb, 0xfde18f12, 0x8fd3a16b, 0x5d718b46, 0x86fdeac9, 0x90eaa887, 
    0xde76b6b4, 0x276d2b6a, 0xe20fae7a, 0x192ceb7d, 0xa27ef047, 0xcb02bfbb, 0xf164f62f, 0x6908798b, 0x5c01ef8f, 0xf33ada29, 0x2a54c89f, 0xc238ee7a, 
    0x97c0ccee, 0x28dcdac2, 0x031bac5d, 0x3f56d7ae, 0x41903590, 0x8a600407, 0xcf26fdf4, 0x9de2d353, 0xe94dafaf, 0xd4411420, 0xd7fadc63, 0x4326ea9c, 
    0xb1b45ca9, 0x66a55d18, 0xc9e19f31, 0x1b652ace, 0x55149c20, 0x4551d4ca, 0x49519841, 0xe12fa5cf, 0xb05f0540, 0x521df5d0, 0xdbce9616, 0xa46d45cd, 
    0xfcc155ef, 0xae50bd4f, 0x9d80c0f3, 0x0f0ee41e, 0xe07f709e, 0xee2aaa22, 0x4e2ac2c5, 0xf0a5e6cc, 0x170ab7b6, 0xebc0066b, 0xe48fd5b5, 0x4110640d, 
    0xbd2218c1, 0xd4b3493f, 0x6ba7f8f4, 0x487ad3eb, 0x18751005, 0xe7b53ef7, 0xea90893a, 0x462c2d57, 0x8c596917, 0x7372f867, 0x908d728a, 0x721505e7, 
    0x505114b5, 0x45511466, 0xc37e1500, 0x4875d443, 0x6f3b5b5a, 0x93b61535, 0xf10757bd, 0xbb42f53e, 0x740202cf, 0x090e907b, 0xf9fee39c, 0x76575115, 
    0x7652112e, 0x852f3567, 0xbb50b8b5, 0x5d073658, 0x207facae, 0x0e82206b, 0xe915c108, 0xa79e4dfa, 0x5f3bc5a7, 0x40d29b5e, 0xc7a88328, 0x39aff5b9, 
    0x52874cd4, 0x306269b9, 0x63cc4abb, 0x9c93c33f, 0x876c9453, 0x95ab2838, 0x838aa2a8, 0x288aa230, 0x1af6ab00, 0x42aaa31e, 0x79dbd9d2, 0x9db4ada8, 
    0x893fb8ea, 0xde15aaf7, 0xa3131078, 0x4c7080dc, 0xc8f71fe7, 0xb1bb8aaa, 0xb3938a70, 0x2d7ca939, 0xda85c2ad, 0xed3ab0c1, 0x03f96375, 0x70100459, 
    0x4faf0846, 0x3df56cd2, 0xfada293e, 0x0192def4, 0x3d461d44, 0xce79adcf, 0x953a64a2, 0x85114bcb, 0x196356da, 0xe29c1cfe, 0x3964a39c, 0xad5c45c1, 
    0x19541445, 0x40511485, 0x09fe7605, 0x488bb7b8, 0x23ae49b9, 0x01c7b48c, 0x45f703dc, 0x45551471, 0xcba8dcd9, 0x34efdc95, 0xd04f336d, 0x35a97b27, 
    0x4805e5b8, 0xf85108da, 0x29d79c8c, 0xa9c5dfaf, 0x42dd336b, 0x0a888d08, 0x0c20c148, 0xa1a8b366, 0x6ec8eaca, 0x28485657, 0x02928aa2, 0x02a0288a, 
    0xdc047fbb, 0x5ca4c55b, 0xc611d7a4, 0xee80635a, 0xb8a2fb01, 0xeca22a8a, 0xca6554ee, 0x369a77ee, 0x13e8a799, 0xdc9ad4bd, 0x6da48272, 0x46fc2804, 
    0xd7946b4e, 0xb5d4e2ef, 0x04a1ee99, 0x2405c446, 0x33069060, 0xe550d459, 0x2b376475, 0x511424ab, 0x45014945, 0x5d015014, 0x2d6e82bf, 0x522ed2e2, 
    0x2de3886b, 0x0077c031, 0x455cd1fd, 0x77765115, 0x77e5322a, 0x4c1bcd3b, 0xde09f4d3, 0x396e4dea, 0x82365241, 0x27237e14, 0xf76bca35, 0xcc5a6af1, 
    0x238250f7, 0x30920262, 0xac190348, 0xba7228ea, 0xd5951bb2, 0xa2280a92, 0x8aa280a4, 0xdfae0028, 0xf11637c1, 0x35291769, 0x989671c4, 0x7e803be0, 
    0x8a22aee8, 0x953bbba8, 0x9dbb7219, 0x69a68de6, 0x75ef04fa, 0xa01cb726, 0x0a411ba9, 0x9a93113f, 0xf8fb35e5, 0x7b662db5, 0xb11141a8, 0x24184901, 
    0x75d68c01, 0x595d3914, 0xc9eaca0d, 0x52511405, 0x00d9ff41, 
};
};
} // namespace BluePrint
