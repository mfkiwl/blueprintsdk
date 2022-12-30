#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <AlphaBlending_vulkan.h>

namespace BluePrint
{
struct AlphaFusionNode final : Node
{
    BP_NODE_WITH_NAME(AlphaFusionNode, "Alpha Transform", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Fusion#Video#Mix")
    AlphaFusionNode(BP* blueprint): Node(blueprint)  { m_Name = "Alpha Transform"; }

    ~AlphaFusionNode()
    {
        if (m_alpha) { delete m_alpha; m_alpha = nullptr; }
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
        float alpha = 1.0f - context.GetPinValue<float>(m_AlphaIn);
        if (!mat_first.empty() && !mat_second.empty())
        {
            int gpu = mat_first.device == IM_DD_VULKAN ? mat_first.device_number : ImGui::get_default_gpu_index();
            if (!m_Enabled)
            {
                m_MatOut.SetValue(mat_first);
                return m_Exit;
            }
            if (!m_alpha || m_device != gpu)
            {
                if (m_alpha) { delete m_alpha; m_alpha = nullptr; }
                m_alpha = new ImGui::AlphaBlending_vulkan(gpu);
            }
            if (!m_alpha)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_first.type : m_mat_data_type;
            m_NodeTimeMs = m_alpha->blend(mat_first, mat_second, im_RGB, alpha);
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
        // if show icon then we using u8"\ue3a5"
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
    FloatPin  m_AlphaIn     = { this, "Alpha" };
    MatPin    m_MatOut      = { this, "Out" };

    Pin* m_InputPins[4] = { &m_Enter, &m_MatInFirst, &m_MatInSecond, &m_AlphaIn };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device        {-1};
    ImGui::AlphaBlending_vulkan * m_alpha   {nullptr};
    mutable ImTextureID  m_logo {nullptr};
    mutable int m_logo_index {0};

    const unsigned int logo_size = 3640;
    const unsigned int logo_data[3640/4] =
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
    0xe6e5e4e3, 0xeae9e8e7, 0xf5f4f3f2, 0xf9f8f7f6, 0x00dafffa, 0x0001030c, 0x11031102, 0xcf003f00, 0xd22b8aa2, 0xb2a22820, 0xc9a9493c, 0xc0e0e9a6, 
    0x809127fb, 0x3a708043, 0xe6f90f9e, 0x60577693, 0xff5cd16b, 0x6bf58600, 0x2daec9bb, 0xc733db2f, 0x53a80cf3, 0x0f60848e, 0x055de7a7, 0x5d49c928, 
    0x15455100, 0x51142d40, 0x15450152, 0x410de29b, 0x3c34edb4, 0x1b69b62f, 0x000e700a, 0x00ffc1eb, 0xca6ed23c, 0x585169e0, 0xeed41dbe, 0xb5b8652f, 
    0x8f6662bd, 0xe3009591, 0x3ffcc0b8, 0x2da2da3a, 0x51005d49, 0x5ac01445, 0x90a4a228, 0xaea58aa2, 0x4c83fd5e, 0x66db4612, 0xb89ec276, 0xe71faf03, 
    0xaeec26ad, 0x9715ed32, 0x5b42fde1, 0xbbb7b8e7, 0xc847e763, 0x8cfb00c8, 0xadf3c30f, 0xb8ba134a, 0x8aa22858, 0x14451d62, 0x45518054, 0x962ed656, 
    0xa1124bc7, 0x01b633db, 0xc5ebc0eb, 0x09aeec26, 0x5045b35c, 0xfd1a2fd0, 0x9fe482a7, 0x23413e36, 0x7f8c7a04, 0x847eb59e, 0x580daeee, 0x608aa228, 
    0x56144547, 0x7a7105c5, 0x9e0ca3fe, 0xac4c8823, 0x18105af0, 0x9f7a522f, 0xed0a3fe8, 0xd14e3b2b, 0x66aed320, 0x966549b9, 0x48f3be59, 0x414fae47, 
    0x5a2ece59, 0xfe96a321, 0x00ffb7d8, 0x1b359984, 0x9c389275, 0x0e7c2881, 0xba9d1ebc, 0xd5baee1a, 0xb748571d, 0xe748e2d5, 0x908d5d67, 0x39fd8cc9, 
    0xe44570ab, 0x676f91c0, 0x3777a1d8, 0x17218a53, 0xe803ee16, 0x42d08aa2, 0x201545d1, 0x7aebc915, 0xc5ae2f8c, 0x163cabe6, 0xee550684, 0x2be89f7a, 
    0x763aaaac, 0x719b0e93, 0x9224e92c, 0x69de2fc9, 0xe8c9f508, 0xb49c262a, 0x756d8e40, 0x1af1768b, 0x4592c05f, 0x0d249013, 0xd383d781, 0xf5c8aef3, 
    0x9a6aaae2, 0x49b43a74, 0x6193c51c, 0x7fc664c8, 0x7e0cb15a, 0xe616294c, 0xe6068a7d, 0x5a8468ea, 0xd1c7e06e, 0x2d601545, 0x48525114, 0xeaab3757, 
    0x1faf3611, 0xb5e0a59a, 0x7aeea83c, 0xa42be89f, 0x9d769aaa, 0x93349b16, 0x4bb2bc23, 0x3d429af7, 0x890a7a72, 0x4643f426, 0xbcbd7a0c, 0xc0defa9a, 
    0x90134592, 0xd7810d24, 0xaef3d383, 0x801886a9, 0x3bf55573, 0x9248f538, 0x874d4939, 0xebaf9321, 0x951f4352, 0xbeb8470a, 0xeae606d0, 0xf76a8a68, 
    0xa2e8631b, 0x5147b28a, 0x15201545, 0xc45fa985, 0xaacc1875, 0xaa5e1b5e, 0x3ff5883f, 0x0d2ab7d2, 0x363dce36, 0x79462549, 0xe4ac4e1e, 0x4cb5e77a, 
    0x3b0d7a93, 0xf66ab118, 0xee82ebf3, 0x9b18de18, 0xeb784002, 0xd7f9e9c1, 0x902be046, 0x0e35400e, 0x716a02a5, 0x36e5722c, 0xbf4e869c, 0xf2243aad, 
    0x16f758a2, 0xdddc00da, 0x5c4d114d, 0x14451f1b, 0xcf6e0855, 0xd37b367a, 0x8599a3e8, 0xf49ecdd8, 0xd1a7f76c, 0xcd6df220, 0x632c9fc4, 0x61e6a8f8, 
    0xbd673376, 0x60e93d1b, 0xa48dba75, 0x3855e787, 0xea940e24, 0x8c5dd839, 0xcf46efd9, 0x73147d7a, 0xb31bbb30, 0xf49e8dde, 0x98792afa, 0xf76cc6ae, 
    0x3ebd67a3, 0x76b4f19c, 0x616800ff, 0x67f1c788, 0x8599a3f0, 0x7acf16d9, 0xe1d37b36, 0x9236e206, 0xe0545920, 0x398ada91, 0xd98c5dd8, 0x7acf46ef, 
    0x3073147d, 0xbd6713bb, 0xd4e93d1b, 0x5d31f354, 0x46efd98d, 0x48757acf, 0xfe6d9160, 0x7f1157d0, 0x47e9237b, 0x1db20b33, 0xf76cf49e, 0xe68d23a9, 
    0x4b8da489, 0xed48f02a, 0x8699a34d, 0xd17b76a3, 0x459ddeb3, 0xc42ecc1c, 0xcf46efd9, 0x3c15757a, 0x766357cc, 0xdeb3d17b, 0x96fd529d, 0xa3fdb36f, 
    0xbdbfee67, 0x99a3f491, 0x6783ec8e, 0xea3d1bbd, 0x4896b764, 0xca42449a, 0x51ed48bd, 0x51c3ccd1, 0xd9e83dbb, 0x8ea24eef, 0x94b12b66, 0x50684551, 
    0xe4555d55, 0x4edb82b8, 0x12f2b085, 0x03e7f449, 0xcbaad5fa, 0x569fb2b5, 0x571a699e, 0x76ec1cb4, 0x0c3a0e52, 0xcb543b72, 0x8d252ab2, 0xf5d1242d, 
    0x8cddac79, 0x3df9288b, 0xb5fec8f8, 0x5c65a573, 0x25e913e9, 0xeff9962e, 0x12fe301c, 0xc691e34a, 0xbe096a4d, 0x4adb04d0, 0x91b6f9ef, 0x9a8e2882, 
    0x8aa28504, 0x8a16892a, 0x5310a928, 0xe2920635, 0x16c2ded6, 0x7312f2da, 0xf507cee9, 0x3b2bd5a7, 0x9946b539, 0xebb4679a, 0x3848d9b5, 0x153906ce, 
    0x7754ec32, 0x2e89692d, 0x69cda991, 0x83841923, 0x23e3f7e4, 0xc1c8d3fa, 0x6933aac1, 0x6e1be932, 0x3edef385, 0x2b27fce1, 0x3519478e, 0x47fbce7b, 
    0xdf95b6ef, 0x04236df3, 0x90c31e51, 0x44154551, 0x5414450b, 0xb64b2a88, 0xdec6e292, 0xf2da16c6, 0xcee97312, 0xd8504705, 0xa4a9c9da, 0x5c6be6a5, 
    0x39cc0aee, 0x1477e01c, 0xb9e3629f, 0xf4494d6f, 0x65adf99b, 0x3e983f63, 0xfe387e4f, 0x1c0411b5, 0x5fa8081e, 0x0b1c6d4b, 0x6dd77e91, 0x8e91db91, 
    0x3b351947, 0x3386f64d, 0x7ce62b05, 0x14c148db, 0x2821c12e, 0x0b648aa2, 0x88541445, 0x4bc6dc2a, 0x280b368d, 0x5bc86b5b, 0x0592439f, 0xf669a754, 
    0xca5ba9cd, 0x9d6b731e, 0x238759c1, 0x14190792, 0xe5868bbc, 0x8d3b3abd, 0x696bee26, 0xfd0bae5d, 0xfe387ecf, 0x0e8248b5, 0xef14c108, 0x51eccfec, 
    0x06ed9ff6, 0xdc8e6cbb, 0xd0647b8c, 0x69970bf3, 0x0ef395c2, 0x3372a4ed, 0x861c764d, 0x191445d1, 0xa2a4a28c, 0x23779cab, 0x8ea2a4a2, 0x151db970, 
    0x85731425, 0x28a9e8c8, 0x322e9ca3, 0x9e8a928a, 0x8a8e5c71, 0xc2398a92, 0x945474e4, 0x2317ce51, 0x8ea2a4a2, 0x2acab870, 0xc2792a4a, 0x945474e4, 
    0x2317ce51, 0x8ea2a4a2, 0x151db970, 0x85731425, 0xd15251c6, 0x912bce53, 0x47d15251, 0x8a8a5c38, 0xc2398a96, 0xb45454e4, 0x2317ce51, 0x862a8aa2, 
    0xdad1a015, 0xa90ba243, 0x9f84cc23, 0x1fd01398, 0x5a7dd64c, 0xcdd2c84b, 0xc83c9164, 0xff4a0ce0, 0x4d927b00, 0x921b2a17, 0x6a5951e9, 0x19aacc90, 
    0xe4844e24, 0x0a55f811, 0xdbaa65b3, 0xf16e57ea, 0x47bc3ddb, 0x0e70f001, 0x2b2a3d78, 0x1237c488, 0x31420e46, 0x1cbad100, 0x283a5297, 0xa20582a2, 
    0x15442a8a, 0x4559b0aa, 0xa2b90ba1, 0x2c319fcc, 0xfa00e718, 0x4aabb266, 0xd3e3b80b, 0xcc134956, 0xa7c4008e, 0x1427b9f7, 0xf4e386cb, 0x553f7dcb, 
    0xbc1a6182, 0x2486876e, 0x65a51f8c, 0xa9869556, 0x214faa6f, 0x88b6674b, 0x010e36e0, 0x549107cf, 0x5c1062ae, 0x823b10c9, 0xd1fa0031, 0x68b624d0, 
    0xa4288a8a, 0x4551b440, 0xa1b68248, 0x044db0b5, 0x67094f5e, 0xc76089f9, 0xcdf60438, 0xa5d5d662, 0xaca345da, 0x47e647b2, 0xfb537200, 0x688a93dc, 
    0xd3452db8, 0x5acdf46c, 0x5e256fda, 0xc4804337, 0x35f48391, 0x7e7a5b89, 0x24af06a9, 0xda5e6d81, 0x5bdb8023, 0x23afe700, 0x08759515, 0x1096bab7, 
    0x503982db, 0x39e8687d, 0x344413ad, 0x31234551, 0x484551d4, 0x6cadb782, 0x05d0e5b4, 0x9c05cced, 0x9bc12c6e, 0xcdf60438, 0x6143d760, 0x22e89074, 
    0x48cc8f57, 0xef4fc955, 0xa48a9360, 0x9aa80569, 0xab93867d, 0xead6ccda, 0x6e1c32f1, 0x867ef024, 0xb4e8fab9, 0x640d4edd, 0xf6688bca, 0xb50d63d1, 
    0xc67d0eb0, 0x11ee0a2b, 0x2a2c776f, 0x546e84db, 0xc11e5a1f, 0x229aac34, 0x4c2a8aa2, 0x6ca7e8c8, 0xbd67a3f7, 0xba8acc69, 0xcb34561b, 0x2b23d3b6, 
    0xce09891a, 0x9c3ccf59, 0x7b76a8f6, 0x8edeb3d1, 0x2d560a64, 0x5e936adf, 0xc6aa10c4, 0xa35e4647, 0x0a2aaaf3, 0x118b05a8, 0xfd34f5dc, 0xf76cf49e, 
    0xca0d9da3, 0x6ca768e3, 0xbd67a3f7, 0x4257c81c, 0xefd94f51, 0x9e7acf46, 0x95a12b64, 0x1fd44c73, 0x8dcb874c, 0x677e9d04, 0xda73f23c, 0xa3f76cab, 
    0xe81cbd67, 0xbdd8566a, 0x79cfac7b, 0x1189cd16, 0x8f7a191d, 0x2080b3ce, 0xeeb15800, 0xd94f534f, 0x7acf46ef, 0x2b07d339, 0x7e8a32ee, 0x7b367acf, 
    0x7485ccd1, 0xcf7e8a36, 0xd47b367a, 0x0c5d21f3, 0xab66faab, 0x95079726, 0x5f276b1c, 0x9c3ccf99, 0xf49e3dd5, 0x9da3f76c, 0x635ba921, 0xb95ff346, 
    0x44608bbb, 0xe465f688, 0xc6b2ce8f, 0x3d16cb71, 0xfb69eac9, 0xefd9e83d, 0xe5609a43, 0x4f51c67d, 0xcf46efd9, 0xae90397a, 0xb39da284, 0xf59e8dde, 
    0x4357c83c, 0xd64c476b, 0x84edd2a5, 0x6392c431, 0x9e13333f, 0xcf866a4e, 0xd37b367a, 0x566a4853, 0xf1bbd5d8, 0x5bd4dd25, 0x3fe23816, 0xfc485ede, 
    0xe29f1feb, 0x3db9c762, 0xbd673f4d, 0x73e83d1b, 0xdc576e4c, 0x9eed146d, 0xa5f76cf4, 0x12ba89cc, 0x02a0288a, 0xf7e8dbae, 0xe2c256f7, 0x7a443718, 
    0xf11df536, 0xeb2ad5dc, 0x7f96a5b4, 0x046f470d, 0x1c8e5c9e, 0xe4747f2b, 0x5c45d5e0, 0x9c9947b8, 0xa32ebafc, 0xda2b4d0c, 0x41eac5b6, 0xaa911f07, 
    0x4559db35, 0xcc5aa67d, 0xf4351297, 0xfa4025a7, 0x92b8e674, 0x0456b10a, 0xed417010, 0xe31c5643, 0x2a8aa260, 0x8aa2c54c, 0xfd2a402a, 0xdea889b6, 
    0x16cc2d5b, 0xe16ce2fb, 0x8eefa8b7, 0x6b57a8e6, 0x78b3a4a2, 0x4f822059, 0x150e572e, 0x72ba00ff, 0xaea26a70, 0xcea42254, 0x530b6d6a, 0x4b961682, 
    0x4abdd846, 0x467e6cb0, 0xb3beebb3, 0x4d00ff86, 0x646e99b1, 0x2ab9f96b, 0x733aa607, 0x0a22b8d6, 0x100456b1, 0x44ed4170, 0xa8388795, 0x54144589, 
    0x51142d90, 0x1bad2052, 0xbe53074d, 0x6b9b5bb6, 0x7036f16d, 0x878e62de, 0x5767cd1d, 0xe5f8e179, 0x1449c29b, 0xb8727912, 0x00ff2390, 0x3538ee74, 
    0xc2655751, 0xe5cc4e2a, 0xab3d3c6e, 0xcbd2425b, 0x9217c166, 0xf9b19555, 0x47afcd1a, 0xd35087b3, 0xd485d974, 0xc94da8ad, 0xe9981e55, 0x96f35acf, 
    0x8155ac52, 0x7b101c04, 0x27d82851, 0x45091b15, 0x2d905414, 0x20525114, 0x0f3f2bad, 0xb285ba6a, 0xf9dadadc, 0xde703691, 0xe3e08c62, 0x5e9b35a1, 
    0xe5a8e187, 0x1ccbc19b, 0xb8527912, 0x00ff2390, 0x3538ee74, 0x70d98d70, 0x39b3938a, 0xeb0d9f0b, 0xd3b49016, 0x19453059, 0x035b5925, 0xebb226f0, 
    0xd421edd3, 0x6f26ed74, 0x8cda4ab5, 0x7d5525e3, 0xb59ed331, 0x58a52ce6, 0x380802ab, 0x71a2f620, 0x1b15e7b0, 0x54144509, 0x8aa28c19, 0xba82622b, 
    0xbb62196d, 0x92b7f1f0, 0x45da2de9, 0x9111cb75, 0x57647b92, 0xec344533, 0xa8636554, 0xf0b4b4b1, 0xfa4c52f4, 0x8eddb8a8, 0x007a6c07, 0x5dce354f, 
    0xb9ab0bdc, 0x8976156e, 0xb607b01c, 0x6f282a6a, 0xacaedca0, 0x22455114, 0x2a8aa205, 0xd1ad2b40, 0x195e3704, 0x475db236, 0xb9ae4803, 0x27b93262, 
    0x547245b6, 0x46c54e53, 0x34ecac5c, 0x0e2f5beb, 0xabaf2c45, 0x70ecc62d, 0x801ee308, 0xb772cd13, 0xf6ee0297, 0xa25d856b, 0xed012c57, 0x4dd15093, 
    0x9512e8bb, 0x288a82d5, 0x285a24a9, 0x5d41a4a2, 0xee6d84a6, 0x49f6297c, 0xa581e22e, 0xc32c1759, 0xc679922b, 0x9ca28b6b, 0xb98c8a5d, 0x7669dc59, 
    0xe1867ffa, 0x7435599a, 0x503e3e9c, 0x50f41847, 0x5fc7354f, 0xf4dd2d5c, 0x56bb0af7, 0xe8010b59, 0xa628a809, 0x946075e5, 0x5114acae, 0xd1224945, 
    0x0a221545, 0xb50634ef, 0x00ffc19f, 0x28ee9262, 0x0991555a, 0x935c1966, 0x1d5c33ce, 0x8a955115, 0xe859b98c, 0x6f1a4c1a, 0x499e6086, 0x389c8475, 
    0x0e20281f, 0xcd13143d, 0x2bf7f770, 0x5c5ca879, 0x6459edaa, 0x13d00366, 0xcaa1e855, 0xae94c3ea, 0x455114ac, 0xd9ff0749, 
};
};
} // namespace BluePrint
