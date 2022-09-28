#include <BluePrint.h>
#include <Node.h>
#include <Pin.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <Crop_vulkan.h>

namespace BluePrint
{
struct MatCropNode final : Node
{
    BP_NODE_WITH_NAME(MatCropNode, "Mat Crop", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Media")
    MatCropNode(BP& blueprint): Node(blueprint) { m_Name = "Mat Crop"; }

    ~MatCropNode()
    {
        if (m_filter) { delete m_filter; m_filter = nullptr; }
    }

    void Reset(Context& context) override
    {
        Node::Reset(context);
        if (m_filter) { delete m_filter; m_filter = nullptr; }
    }

    void OnStop(Context& context) override
    {
        m_mutex.lock();
        m_MatOut.SetValue(ImGui::ImMat());
        m_mutex.unlock();
    }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        if (entryPoint.m_ID == m_IReset.m_ID)
        {
            Reset(context);
            return m_OReset;
        }
        auto mat_in = context.GetPinValue<ImGui::ImMat>(m_MatIn);
        if (!mat_in.empty())
        {
            if (!m_Enabled)
            {
                m_MatOut.SetValue(mat_in);
                return m_Exit;
            }
            if (m_width != mat_in.w || m_height != mat_in.h)
            {
                m_width = mat_in.w;
                m_height = mat_in.h;
                m_x1 = 0; m_y1 = 0;
                m_x2 = m_width;
                m_y2 = m_height;
            }
            if (m_x2 - m_x1 <= 0 || m_y2 - m_y1 <= 0)
            {
                return {};
            }
            int gpu = mat_in.device == IM_DD_VULKAN ? mat_in.device_number : ImGui::get_default_gpu_index();
            if (!m_filter)
            {
                m_filter = new ImGui::Crop_vulkan(gpu);
                if (!m_filter)
                    return {};
            }
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            im_RGB.w = mat_in.w;
            im_RGB.h = mat_in.h;
            m_NodeTimeMs = m_filter->cropto(mat_in, im_RGB, m_x1, m_y1, m_x2 - m_x1, m_y2 - m_y1, m_xd * im_RGB.w, m_yd * im_RGB.h);
            im_RGB.time_stamp = mat_in.time_stamp;
            im_RGB.rate = mat_in.rate;
            im_RGB.flags = mat_in.flags;
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
        int _x1 = m_x1;
        int _y1 = m_y1;
        int _x2 = m_x2;
        int _y2 = m_y2;
        float _xd = m_xd;
        float _yd = m_yd;
        // TODO::Hard to get focus and input number
        static ImGuiSliderFlags flags = ImGuiSliderFlags_None;//ImGuiSliderFlags_NoInput;
        ImGui::Dummy(ImVec2(300, 8));
        ImGui::PushItemWidth(300);
        ImGui::BeginDisabled(!m_Enabled);
        ImGui::SliderInt("x1##Crop", &_x1, 0, m_width, "%d", flags);
        ImGui::SameLine(320); if (ImGui::Button(ICON_RESET "##reset_x1##Crop")) { _x1 = 0; }
        ImGui::SliderInt("y1##Crop", &_y1, 0, m_height, "%d", flags);
        ImGui::SameLine(320); if (ImGui::Button(ICON_RESET "##reset_y1##Crop")) { _y1 = 0; }
        ImGui::SliderInt("x2##Crop", &_x2, _x1, m_width, "%d", flags);
        ImGui::SameLine(320); if (ImGui::Button(ICON_RESET "##reset_x2##Crop")) { _x2 = m_width; }
        ImGui::SliderInt("y2##Crop", &_y2, _y1, m_height, "%d", flags);
        ImGui::SameLine(320); if (ImGui::Button(ICON_RESET "##reset_y2##Crop")) { _y2 = m_height; }
        ImGui::SliderFloat("xd##Crop", &_xd, -1.f, 1.f, "%.02f", flags);
        ImGui::SameLine(320); if (ImGui::Button(ICON_RESET "##reset_xd##Crop")) { _xd = 0.0f; }
        ImGui::SliderFloat("yd##Crop", &_yd, -1.f, 1.f, "%.02f", flags);
        ImGui::SameLine(320); if (ImGui::Button(ICON_RESET "##reset_yd##Crop")) { _yd = 0.0f; }
        ImGui::PopItemWidth();
        if (_x1 != m_x1) { m_x1 = _x1; changed = true; }
        if (_y1 != m_y1) { m_y1 = _y1; changed = true; }
        if (_x2 != m_x2) { m_x2 = _x2; changed = true; }
        if (_y2 != m_y2) { m_y2 = _y2; changed = true; }
        if (_xd != m_xd) { m_xd = _xd; changed = true; }
        if (_yd != m_yd) { m_yd = _yd; changed = true; }
        ImGui::EndDisabled();
        return changed;
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
        if (value.contains("width"))
        {
            auto& val = value["width"];
            if (val.is_number()) 
                m_width = val.get<imgui_json::number>();
        }
        if (value.contains("height"))
        {
            auto& val = value["height"];
            if (val.is_number()) 
                m_height = val.get<imgui_json::number>();
        }
        if (value.contains("x1"))
        {
            auto& val = value["x1"];
            if (val.is_number()) 
                m_x1 = val.get<imgui_json::number>();
        }
        if (value.contains("y1"))
        {
            auto& val = value["y1"];
            if (val.is_number()) 
                m_y1 = val.get<imgui_json::number>();
        }
        if (value.contains("x2"))
        {
            auto& val = value["x2"];
            if (val.is_number()) 
                m_x2 = val.get<imgui_json::number>();
        }
        if (value.contains("y2"))
        {
            auto& val = value["y2"];
            if (val.is_number()) 
                m_y2 = val.get<imgui_json::number>();
        }
        if (value.contains("xd"))
        {
            auto& val = value["xd"];
            if (val.is_number()) 
                m_xd = val.get<imgui_json::number>();
        }
        if (value.contains("yd"))
        {
            auto& val = value["yd"];
            if (val.is_number()) 
                m_yd = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) const override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["width"] = imgui_json::number(m_width);
        value["height"] = imgui_json::number(m_height);
        value["x1"] = imgui_json::number(m_x1);
        value["y1"] = imgui_json::number(m_y1);
        value["x2"] = imgui_json::number(m_x2);
        value["y2"] = imgui_json::number(m_y2);
        value["xd"] = imgui_json::number(m_xd);
        value["yd"] = imgui_json::number(m_yd);
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }

    FlowPin   m_Enter   = { this, "Enter" };
    FlowPin   m_IReset  = { this, "Reset In" };
    FlowPin   m_Exit    = { this, "Exit" };
    FlowPin   m_OReset  = { this, "Reset Out" };
    MatPin    m_MatIn   = { this, "In" };
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[3] = { &m_Enter, &m_IReset, &m_MatIn };
    Pin* m_OutputPins[3] = { &m_Exit, &m_OReset, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    ImGui::Crop_vulkan * m_filter {nullptr};
    int m_x1 {0};
    int m_y1 {0};
    int m_x2 {0};
    int m_y2 {0};
    float m_xd {0};
    float m_yd {0};
    int m_width {0};
    int m_height {0};
};
} //namespace BluePrint