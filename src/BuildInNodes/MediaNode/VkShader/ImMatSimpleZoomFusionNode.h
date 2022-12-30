#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <SimpleZoom_vulkan.h>

namespace BluePrint
{
struct SimpleZoomFusionNode final : Node
{
    BP_NODE_WITH_NAME(SimpleZoomFusionNode, "SimpleZoom Transform", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Fusion#Video#Move")
    SimpleZoomFusionNode(BP* blueprint): Node(blueprint) { m_Name = "SimpleZoom Transform"; }

    ~SimpleZoomFusionNode()
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
                m_fusion = new ImGui::SimpleZoom_vulkan(gpu);
            }
            if (!m_fusion)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_first.type : m_mat_data_type;
            m_NodeTimeMs = m_fusion->transition(mat_first, mat_second, im_RGB, progress, m_quickness);
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
        float _quickness = m_quickness;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        ImGui::Dummy(ImVec2(200, 8));
        ImGui::PushItemWidth(200);
        ImGui::SliderFloat("Quickness##SimpleZoom", &_quickness, 0.1, 3.f, "%.1f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_quickness##SimpleZoom")) { _quickness = 0.8f; changed = true; }
        ImGui::PopItemWidth();
        if (_quickness != m_quickness) { m_quickness = _quickness; changed = true; }
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
        if (value.contains("quickness"))
        {
            auto& val = value["quickness"];
            if (val.is_number()) 
                m_quickness = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["quickness"] = imgui_json::number(m_quickness);
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
        // if show icon then we using u8"\ue8c4"
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
    float m_quickness   {0.8f};
    ImGui::SimpleZoom_vulkan * m_fusion   {nullptr};
    mutable ImTextureID  m_logo {nullptr};
    mutable int m_logo_index {0};

    const unsigned int logo_size = 5025;
    const unsigned int logo_data[5028/4] =
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
    0xe6e5e4e3, 0xeae9e8e7, 0xf5f4f3f2, 0xf9f8f7f6, 0x00dafffa, 0x0001030c, 0x11031102, 0xcf003f00, 0xd22b8aa2, 0xa9a22820, 0x00fff76a, 0xb76cbad9, 
    0x18756923, 0x760bdd40, 0xc095dda4, 0xda7345bb, 0x6fae881f, 0x7bcbbeef, 0x4d177114, 0xb952edd1, 0x57fadcef, 0x52324a43, 0x51144057, 0x14055045, 
    0x14054051, 0x35894756, 0x38dd3439, 0x79b20f34, 0x07380418, 0xffe0a903, 0x37699e00, 0xbd067665, 0x7bf8cb15, 0xd4b8be5b, 0x49a8cb7e, 0x748fcdbb, 
    0xfdbe5079, 0xabfc7187, 0x2919a5a8, 0x280aa02b, 0x8a02a8a2, 0x8a02a028, 0x9ac4232b, 0x9c6e9a9c, 0x3cd9071a, 0x031c028c, 0x7ff0d481, 0xb29b34cf, 
    0x8a5e03bb, 0xad3dfce5, 0x3f6a5cdf, 0xdd24d465, 0x3cbac7e6, 0xc37e5fa8, 0xd455feb8, 0x95948c52, 0x511405d0, 0x14450154, 0x15450150, 0xe7abe289, 
    0xc3c2d2b3, 0xae2c472b, 0x38389402, 0x49699e1c, 0xba0d5cd9, 0xe6f08e2b, 0xa31675ab, 0x4a5cdfe4, 0xcf2e32eb, 0x4872cb35, 0x837b32c6, 0x4b288d5d, 
    0x8a025c99, 0xa200aa28, 0xae00288a, 0xc117c647, 0x25b2e0ae, 0xc8c3b4bc, 0x00ff7c07, 0x759dbff5, 0x890e9dd5, 0xeab35a04, 0x99645946, 0x0363c3f3, 
    0x717a9c3e, 0xace43451, 0xff4a8e86, 0x7db55700, 0x4ff3d64a, 0x000c488a, 0x6400292b, 0xa70f9d0e, 0x2ac3dd15, 0x194b0a4f, 0x07863aca, 0x51831ad4, 
    0xedd4e1d3, 0x82e5b40d, 0xd42b0892, 0xb14ee911, 0x381b5bb4, 0x754792ed, 0xc617608c, 0x8c52f871, 0xc5025b5c, 0x085a5114, 0x008aa228, 0x05f18f2b, 
    0x23bebced, 0xdb678ab6, 0x631eb10d, 0x9e7a9e60, 0x2bec2a3f, 0x91c3cb16, 0xd53d7bc3, 0xd378c7eb, 0x858b8773, 0x4deac939, 0x88de3467, 0x35d5c168, 
    0x8bed2f2b, 0x4b19db3b, 0x8d4341ac, 0xff147085, 0xba5d5100, 0xa92bba32, 0x35c8b0ca, 0x7ef8a897, 0xc3d0f6ca, 0x49db100c, 0x10894490, 0xab587e04, 
    0xd9ab759a, 0x246d4558, 0xda1053a2, 0x3b322e18, 0xdd240551, 0x1445b3c0, 0x280a8256, 0xe30a80a2, 0x6f7b41fc, 0xa2ed882f, 0x6cc3f699, 0x27d89847, 
    0xca8fa79e, 0xb2c50abb, 0xde70e4f0, 0xf17a75cf, 0xe1dc34de, 0x724ee1e2, 0xcd59937a, 0x301aa237, 0xcb4a4d75, 0xf6ce62fb, 0x10eb52c6, 0x5c61e350, 
    0x57d43f05, 0x8aae8c6e, 0x32ac72ea, 0x3eea650d, 0xb4bdb21f, 0x3604c330, 0x221164d2, 0x961f0144, 0x6a9de62a, 0x5b1156f6, 0xc4942849, 0x8c0b8636, 
    0x4941d48e, 0xd12c7037, 0x82a01545, 0x02a0288a, 0x536bbdb8, 0xedd7bcb5, 0x2dd8be83, 0x27d80e9f, 0xf8f1dc90, 0xdfa5ec0a, 0x01943fe5, 0x6368a77d, 
    0xcdd59e81, 0x2213be69, 0x513559e2, 0x50b9c11c, 0xc973dd8e, 0x8a2a2b3d, 0x190dc94e, 0x9de5fd1a, 0x1617ecc5, 0x18d91613, 0xda95f253, 0xf6f9a757, 
    0xe5cee2ae, 0xb8e2ec6e, 0x6a28924e, 0x9ef0bdc8, 0x2db3259f, 0xbc4c3ea4, 0x46ec6d95, 0x2739c071, 0xecd05cbd, 0xe534acee, 0x6264bab5, 0x6294768c, 
    0x52dceb78, 0xbf939282, 0xa2683450, 0x4541d88a, 0x45015014, 0xd4564b15, 0xd7609746, 0xa9303f46, 0x67ec2600, 0x7693d627, 0xa25d6057, 0x149f74b0, 
    0xb67ca72e, 0x96e46bad, 0xbb794352, 0x606c47b2, 0xc928f556, 0x51005d49, 0x14401545, 0x14004551, 0x221e5951, 0xacd324d5, 0x26fb904f, 0x1cdcb87f, 
    0x7df08c60, 0xaeec268d, 0xb8a2d7c0, 0xbac4338d, 0xe976ea93, 0x17be7375, 0x14bbac70, 0x2baee375, 0x863204b6, 0xa9c8081d, 0x18b6d484, 0x584551b4, 
    0xa0288a82, 0xe29aa302, 0x3c0f640b, 0xe0a4c4d1, 0x3fa36017, 0x8f5c498d, 0xbc036f8c, 0xc3f4a076, 0x85f79f91, 0xaecae544, 0x9eba4807, 0x6f15ec9e, 
    0xe09899ad, 0x4f922a01, 0xc88156e7, 0x462cafc8, 0x508e2578, 0x97216508, 0x367aa53f, 0xbdf67593, 0x05090936, 0xae6f28b6, 0xd4696a06, 0xa28dd1e6, 
    0x5a5114ed, 0x8aa22888, 0x56132b00, 0x647a24f1, 0xe76f8988, 0xde1f5892, 0xec31c66d, 0x1157626b, 0x01c7eec4, 0xce6b9254, 0xb4af6b75, 0x65774cdf, 
    0xa7b7af44, 0x6655d64c, 0x371ab4e2, 0xff38e1bf, 0xfe77a800, 0xec00ff47, 0xd54b736b, 0x2c22d5a1, 0xa92a2b80, 0x090edc65, 0x9f57faed, 0x513c65c9, 
    0x6487ac24, 0x1e1cb88b, 0x6be1ad95, 0x78b9b6c5, 0x5ef3edcb, 0xe4398e31, 0x658d00ff, 0xb4e6920a, 0xa2b96387, 0x45337280, 0x51141275, 0xc8150045, 
    0xeee5a6f8, 0xb47448f5, 0xff9c12e0, 0xdb73b400, 0xe2aaaef1, 0x246f6f61, 0x2c35d2cd, 0x015c017f, 0xd7b8bbb9, 0xb7b4be64, 0x1612a469, 0x312c5555, 
    0x2bfc74d0, 0x0d4baf1a, 0x9ddbdc12, 0x7eb45907, 0xb1232a58, 0xf28f3dc6, 0x4edbdc35, 0x4eb136b7, 0xc34024bd, 0xeac315f1, 0xf5bd5ad7, 0x0f9637b8, 
    0xdf416814, 0xa8e3bae4, 0xd0b57eea, 0x5cf04ef8, 0xfb090b69, 0xbd5eb1f0, 0xd4fa0747, 0xa44969d3, 0x8aa2dc0c, 0x4541a02b, 0x5c015014, 0xea33bf26, 
    0x53b5f93a, 0x5e23c8c2, 0xff0027f8, 0x75ec2a00, 0xe9b28f3b, 0x48f713b7, 0xd55e1fc9, 0xc6f7b6e7, 0x145f501d, 0x4d8644de, 0xf749c6b9, 0x644b2bac, 
    0xb4d65e34, 0xd5dad2d6, 0x9251a9d7, 0xfa02ea01, 0xa25fd7fb, 0x1ead8b5c, 0xf27940da, 0xc7513fc0, 0x544faef4, 0xedd421f1, 0xec5abb0d, 0x33f346cf, 
    0xf6f4418e, 0xd4109eab, 0x6f362515, 0xe46c7cc1, 0x4c4db47f, 0x363d1525, 0xf475e863, 0x92744551, 0x00455114, 0x30046932, 0x29a287bc, 0x3baf903f, 
    0xb5d477bb, 0xdf4952a6, 0x05f64531, 0xd7f5fabf, 0x5af89a78, 0xfc808c69, 0x769cfcd2, 0x8e6b9eca, 0xb3e2eed2, 0xd5895bbe, 0x8007299c, 0xef41ce09, 
    0x6d9ad55c, 0xe2ad9122, 0x346d2d2d, 0x391159fb, 0x07624455, 0xabf61c7c, 0x05bf0f5e, 0x91dbcd5e, 0xbe3d1997, 0x538b8a51, 0xfd8d35f1, 0x12f9db8b, 
    0x2ced20ee, 0x4070808b, 0xfa58efef, 0x35d9d72d, 0x0c385f38, 0xb8e9a7ca, 0x3425f354, 0x1e3d74d0, 0x246e6c8a, 0x861c5c8d, 0x75758a00, 0x47511492, 
    0xd7cb004a, 0x84b505af, 0x4806bb89, 0x9f9e0e9c, 0xede07afd, 0xb89b6ba2, 0xc9bc1bd3, 0xad893f00, 0x226a14bf, 0xa418d679, 0x2bd296cf, 0xfe403d8e, 
    0x36424795, 0x9e2476eb, 0x21924044, 0x7a6fdb05, 0xa7f9a8e4, 0xead19162, 0x58346eba, 0xdb3027c2, 0x943d79b6, 0xf671857f, 0xc87b9b92, 0x241b39a4, 
    0xae813f56, 0xd354e3fa, 0x2392b525, 0x850c0179, 0x71afe671, 0xe802aef5, 0x4698bb20, 0x6d234141, 0x3c63e420, 0xa6c95651, 0x67951e81, 0x455adc3a, 
    0x8dee9c22, 0x3d15f35b, 0xf9627860, 0x1731b666, 0x5c238dc9, 0xc27fe871, 0x2ea6ebb7, 0x1485e4ea, 0x7b055451, 0xb3bf35eb, 0xd1ddd592, 0x0e25c064, 
    0xa5af5a0f, 0x4afad6e8, 0xfbcec2b2, 0x677c3989, 0xaaf6a0b7, 0x68bb235e, 0x41512eec, 0x6cc8db18, 0x48c41577, 0xb10b49e6, 0x9a539f24, 0x651475c2, 
    0x3e3dd2b0, 0xad2edde6, 0x2193b764, 0x8ea45264, 0x997ed6bc, 0x57e95ba0, 0x99676806, 0xb94aa5b2, 0x831ee718, 0x6b51b8da, 0x7f3f5f9b, 0x547c8fe1, 
    0xa1e26004, 0x43bb57d5, 0x56d4eab1, 0xda6e8707, 0x9c238a44, 0x5bc6b82d, 0x3abe15d3, 0xd55d4c57, 0x0d290ac9, 0xedd5ba72, 0xc8b45ac4, 0xdc466192, 
    0xa2fba30d, 0xaecaa594, 0x65bdb606, 0x7d5aa877, 0x0d60db9a, 0x3b928bf7, 0xe9033a82, 0x93fef04d, 0xc26e9726, 0xc646a662, 0x131ce776, 0x02aef5eb, 
    0xabf6e68a, 0x58c59a9b, 0x13e239f5, 0xd14d2441, 0x45fca9d4, 0x1aded672, 0x52d6b4bd, 0x12131ae9, 0xb60cb84b, 0xe9e9e776, 0xd272f850, 0x473e8d0b, 
    0x25cc3d96, 0xb0233923, 0x2b0bb5a8, 0x5691be75, 0xe78e013c, 0x7c9256d0, 0x1da149c9, 0x7a06ceb8, 0x52535ed1, 0x48f1cadb, 0x310e0e59, 0xf257a5d2, 
    0x45a31e0b, 0x7d148579, 0x6b2cc863, 0x5cf8aef8, 0xc008f95f, 0x070763ac, 0xacc202a3, 0xb618de68, 0xdc13b2bf, 0xe89632c9, 0x80318010, 0xc07a8f7b, 
    0xe33f98ad, 0x07f73fde, 0x25e3acf2, 0xcdd03629, 0xf442f86f, 0x75eff9df, 0xaf7d00ff, 0x5c1300ff, 0x7460adbe, 0x960af3bb, 0x24643eda, 0x3f908cf3, 
    0xff596e95, 0x1ff4c700, 0x9d7fd1f5, 0x24a79575, 0x1556b464, 0xde0ebfcc, 0x3f2aad0b, 0x0246739b, 0x45d1731e, 0xb16e566a, 0x7f2c00ff, 0xb3d25fe0, 
    0x686b4e2b, 0x6ea8a523, 0xcbda10e6, 0x8cda3821, 0xc715f473, 0xc700ffd9, 0xd1f51fec, 0x35759d7f, 0xb6ccad11, 0xbb3ccf15, 0x79f63a7f, 0x62e43b07, 
    0xe969f23d, 0xa1c85fa6, 0x94e5c692, 0x6222828c, 0xd0abfc08, 0x7b79ac28, 0x3fe0988f, 0xfa2fb5b2, 0xdf7fdd07, 0x0a00ff96, 0xab3b6b8e, 0xd3f60c70, 
    0xd09b9e45, 0xf43a7fae, 0x41e4283a, 0x055a71cc, 0x91efd8f7, 0x224b2009, 0x7f3d8003, 0x2ae8f5fa, 0xe43094c1, 0xea549111, 0x93ea7ffc, 0x535bd1fd, 
    0x6cc2aa7c, 0xa81d236d, 0x9666d05b, 0xb2c95fab, 0xe28c73ce, 0xaea13f26, 0xf18a3386, 0x5afdce1d, 0x3b6336e5, 0x22fbcf79, 0xcf8be0b2, 0xb7dd6399, 
    0x929af13d, 0xd9cbfcfd, 0x38ce181f, 0xf05d05fc, 0x5bf51bfc, 0xee8cdd76, 0xaec9feed, 0x1b658a0f, 0x3c3bb6d6, 0x67e6ab30, 0xf61ffe19, 0x53d495eb, 
    0x7391becb, 0x0af9639c, 0xfd8b64f4, 0xc79f4714, 0xf8c3f51e, 0xaf66df8e, 0x8db3db72, 0x15d9bfbd, 0x28da58b5, 0x0d00ff10, 0x0eeae54f, 0x24f7e776, 
    0xdc15f775, 0x46a1dbc4, 0xd7a032c6, 0x96274797, 0xc578c6d9, 0x3f8f267a, 0xbb6c6199, 0xf5108871, 0xcbe814f6, 0xd9fb98a0, 0x9d016611, 0xd7dcdb41, 
    0x6d30af0d, 0x11a47266, 0xdd3ff2f2, 0x97ebd215, 0x08a89728, 0xa763f56f, 0xa827d7d4, 0x46fa9238, 0x5fc7c119, 0xfaaca9a0, 0xeeaa150d, 0x4cf78c94, 
    0x871d4b11, 0x55dc3380, 0x14ec5234, 0xe1b93575, 0xd1fe679b, 0x00ff9593, 0xac987f52, 0x74c6aea0, 0x583a0aba, 0x7c2a75b8, 0xec18c1c3, 0x1ca3b62a, 
    0x08217d8d, 0x1ec871c4, 0x10adb5c2, 0x1f0711c7, 0x6836b57b, 0x082ce519, 0x69cbaec1, 0xa2324f62, 0x141d78a4, 0xd20323b1, 0x8902aea3, 0x428aa2e8, 
    0x346ceb0a, 0x85b124e8, 0x6a4cbed8, 0xe40afb7a, 0x3fd3b7ab, 0xff6f1be4, 0xf9975c00, 0xd492d60a, 0xd669db42, 0x5236e6e2, 0xaf0757f9, 0x83116cbd, 
    0xdb6ea38a, 0x7c8c41a6, 0xc3b0afc2, 0x42515d11, 0x2c10ea28, 0x46e25ede, 0xaaf0d333, 0xfb8f627f, 0x8dd6f9cd, 0x357ef0df, 0x815a935a, 0x00ffd9cc, 
    0xf51fecc7, 0x759d7fd1, 0xff59cb35, 0x1fecc700, 0x9d7fd1f5, 0x4fc93575, 0xdaad4262, 0x33b11441, 0xea83e43a, 0xb05fa56a, 0xdf5000ff, 0xd94a7fef, 
    0x361c090d, 0x481182b3, 0x27d5dc23, 0x9ccbae5d, 0x2db4e2e0, 0x506216ee, 0xa863b045, 0x31e24a35, 0xe4a0cc14, 0xd591c150, 0xd500fff8, 0xa8a2fb27, 
    0x00fff855, 0xa2fb27d5, 0x95de4884, 0xf82be281, 0xc96d59e1, 0x9c5bd25d, 0x721ee07c, 0x12e3563a, 0x22c18154, 0x0d5fb5b0, 0xb372a9b6, 0xaa8ce0b5, 
    0x3d8bc004, 0xabf7f5c9, 0x0a6d559d, 0x5d134752, 0x6dee871f, 0xcbbbc5ad, 0xdd151271, 0x3a06ce90, 0x84f0dfd4, 0x47d400ff, 0xff2000ff, 0x7f54f600, 
    0x00ff13c2, 0x83fc1f51, 0x57d900ff, 0xeeca6534, 0xbf373257, 0xfc07acb6, 0xdf7f5bbe, 0x103face5, 0x5adcdadd, 0x61ae185c, 0x55b86d76, 0x53e44970, 
    0xff09e17f, 0xfe8fa800, 0xec00ff41, 0x8400ffa8, 0x3fa2fe27, 0x00ff07f9, 0x5525adb2, 0x9a0b6bb5, 0x675d2d27, 0xcc106f86, 0x5841f923, 0xa6f4bd3f, 
    0xff13c27f, 0xfc1f5100, 0xd900ff83, 0xc33bbd55, 0x345ca70d, 0xb21c7ac9, 0xdc87c16d, 0x547bef1f, 0x1cac5c42, 0x4be29b91, 0x638c9add, 0x87eecfcb, 
    0x1258737f, 0x5fcae6c9, 0xed6acf18, 0xfa22fc6f, 0x9a2ceb84, 0xbf5d088e, 0xbfcf3dea, 0x00ff55bd, 0xa2fe0f84, 0xff03f95f, 0x9da9b200, 0xc473d844, 
    0xf3e3ece5, 0x8c33916e, 0xbac29fe7, 0x795a0f8f, 0xe777ee77, 0xf77444f7, 0x82af2d15, 0xcac2bd45, 0x73fb0d75, 0xf18c93c7, 0xda6bf5fe, 0x53d2e268, 
    0x5792cf22, 0x00ff731f, 0x714e57af, 0x25ce895b, 0xe36ad7a8, 0xd28c94de, 0x00ff0ff9, 0xdaf17f3d, 0x00ff973c, 0xedf8bf9e, 0x7d7a7b74, 0x0f8ecec5, 
    0xcf7ed350, 0x76be2463, 0x6ee3b8ed, 0xc57a8f3b, 0xf0bd44af, 0x3c5adef2, 0xfc66e302, 0x8c33db7c, 0xb2667a10, 0xff03e17f, 0xfe97a800, 0xec00ff40, 
    0x8d7292ab, 0x279e2bf4, 0xa85b5331, 0xf400d96d, 0xffa82bea, 0xfe0f8400, 0x03f95fa2, 0xa9b200ff, 0x9747f022, 0xe798f89f, 0xffc3f43f, 0x53a8b200, 
    0x7ba4bd88, 0x9f96cf9c, 0xbbca5fdc, 0xe92a3c5d, 0xff62e3f6, 0xde5eaa00, 0x00ffa9c2, 0x00ff19c2, 0x83fc0f51, 0x56d900ff, 0x936f1acc, 0x765e1c6f, 
    0x3b1788ed, 0xad0f9c71, 0x4fb00a6d, 0x3d522756, 0xe38dd4c8, 0x4a9fa308, 0x789a9c98, 0xcc07c1b2, 0x3fcdfee9, 0xed00ffec, 0xb7af957e, 0xf15cdca7, 
    0xc686ba2b, 0x4d513f40, 0x8bfbd3f2, 0x6da355f9, 0x29fde39f, 0xdffecb3e, 0xa7b747e9, 0x70e239dc, 0xfbf17f76, 0x5f74fd07, 0x434d5de7, 0xa97c840f, 
    0x67b74f92, 0x95c70663, 0x5aeb07d7, 0xf47fd9bf, 0x1d00ffdb, 0xd7eb00ff, 0x6e452a2c, 0x8512cfc5, 0x40d3054d, 0x938342a5, 0xf6cf6a9e, 0xff36fd5f, 
    0xfa7fc700, 0xff657ff4, 0xfc6fd300, 0xaf00ff77, 0x77687b5a, 0x3a223a17, 0xaa708484, 0x6631b32a, 0xf524c72c, 0x2ffb7b35, 0xe37f9bfe, 0x3f7afdbf, 
    0xe900ffb2, 0xff3bfeb7, 0xdba3d700, 0xd1f9b843, 0xfec7af42, 0x14dd3fa9, 0x00ff657f, 0x77fc6fd3, 0x56af00ff, 0x856ad716, 0xa563d0df, 0x8b3bb40a, 
    0x282a259e, 0xa702a4a2, 0xbab9b1b7, 0xb9c7f042, 0x803bc141, 0x6e0575fe, 0xcbd93ce8, 0x00ffef8f, 0xb12b4d41, 0xb66676a5, 0x588a7c95, 0x083d70c0, 
    0x57aad63f, 0xd50d694f, 0x639eb9ba, 0x0f743f39, 0x9773bdfe, 0xa5cc8d3b, 0x2416a3d0, 0x5643eda9, 0xa223561b, 0x5912298a, 0x032b8aa2, 0x9a76ab30, 
    0x66f4ed5d, 0xc57c684b, 0xc09db453, 0xa99af873, 0x3285675d, 0xebc0d974, 0x2ae49fe6, 0x54ecaea2, 0x478cdd55, 0xd04875d0, 0x07365ab3, 0xfc4959f7, 
    0x30f2ac81, 0xd3b96b70, 0x23adafed, 0x9bdcdd94, 0x8eea7ea2, 0xc635f1a3, 0xb337975e, 0xd99891b4, 0xff5062a4, 0x714e0e00, 0xe5b0514e, 0x280a221b, 
    0xc582a0a2, 0x92595114, 0x45c77a15, 0x9a2835d4, 0x317f4b5b, 0xbd93b615, 0xaa893f47, 0x20f8db35, 0x005ca413, 0x8fe37c70, 0x4555e4fb, 0x47a8d85d, 
    0xe49cd999, 0x62d6199e, 0xd862ed42, 0xb1ba761d, 0xb2ac81fc, 0x1a3c3808, 0xd32e9df4, 0x668eb152, 0x8437bcbd, 0x51075180, 0xf35a9ff8, 0x75c840ed, 
    0x2396861b, 0xc6acb413, 0x3939fc33, 0xc54639c5, 0xaf282a4e, 0x19541445, 0x2a8aa28f, 0xc37e1544, 0x4875d443, 0x7f3b5b5a, 0x93b61535, 0xf10757bd, 
    0xcfbb4235, 0x7b740202, 0x9c090e90, 0xc000ffe0, 0xdd555445, 0x9d54848b, 0xe1cbcc99, 0x2e146e6d, 0xd7810df6, 0xc81fab6b, 0x8320c91a, 0x7a453082, 
    0xa967937e, 0xd74ef1e9, 0x90f4a6d7, 0x31ea200a, 0xce6b7dee, 0xd4211375, 0x8c585aae, 0x18b3d22e, 0x39b9fbc8, 0xc84639c5, 0xb98a8273, 0xa8288a5a, 
    0xa2e83233, 0x0a37ed8a, 0x8d24d1db, 0x9157e46c, 0x71ee9754, 0x142556d8, 0x63a76127, 0xdfd6ce7a, 0x6ba5914f, 0x638c61e5, 0x59e70f20, 0xcdadf337, 
    0xf9a092e4, 0xabde034f, 0x6fb04dd1, 0x5114054b, 0x289a4548, 0x0a33b0a2, 0x342f3cea, 0x24d3e931, 0x21132293, 0x6107ccfb, 0x4915bd5c, 0xbba8dcd9, 
    0x9b669d3b, 0x4aa46763, 0x23aa0fd7, 0x08dab88d, 0xe664f951, 0xb87b35b0, 0x672975ef, 0x00877c88, 0x3306be67, 0xcaa1a854, 0x75e5c0ea, 0x2a8aa260, 
    0x45512c49, 0x5d219915, 0x206e8397, 0x49b84a8f, 0xd3328e67, 0x3f60061c, 0x451b5784, 0xca9d5d54, 0xce5db98c, 0x6cd348df, 0xe99e2934, 0x05e498f5, 
    0x5108da48, 0xb9e664f9, 0x2dfa727d, 0xe69e5747, 0x406c4410, 0x00324652, 0x14856ac6, 0x6e585d39, 0x45415657, 0x3e905414, 0x10a9288a, 0x9be06f57, 
    0x8bb4788b, 0x8eb8e698, 0x071cd322, 0x57843f70, 0x5d544511, 0xb98cca9d, 0x46f3ce5d, 0x02fd34d3, 0x5b93ba77, 0x8d54508e, 0x881f85a0, 0x9a72cdc9, 
    0x965afcfd, 0x20d43db3, 0xa480d888, 0xc600128c, 0x1c8a3a6b, 0xe586acae, 0x8a826475, 0xff20a928, 0x000000d9, 
};
};
} // namespace BluePrint
