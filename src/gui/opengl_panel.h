/*
Decade
Copyright (c) 2019-2022 Marco Peyer

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <https://www.gnu.org/licenses/>.
*/


#pragma once


#include "../graphics/graphics_engine.hpp"
#include "../graphics/mvp_matrices.hpp"
#include "../graphics/render_to_png.hpp"
#include "../packages/page_config.h"

#include "wx_widgets_include.h"
#include <wx/glcanvas.h>

#include <glad/glad.h>

#include <sigslot/signal.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/io.hpp>

#include <memory>
#include <array>
#include <string>



class MouseInteraction
{
public:

    MouseInteraction() :
        persistent_mouse_pos(0.f),
        persistent_scale_factor(1.f),
        translate_pre_scaled(0.f),
        translate_post_scaled(0.f)
    {}

    void Interaction(MVP& mvp, wxPoint mouse_position, bool dragging, int wheel_rotation)
    {
        glm::vec3 current_mouse_pos = MouseWorldSpacePos(mouse_position, mvp);

        if (dragging)
        {
            translate_pre_scaled += current_mouse_pos - persistent_mouse_pos;
        }

        if (wheel_rotation)
        {
            const float mouse_wheel_step = 1200.f;
            const auto scale = static_cast<float>(wheel_rotation) / mouse_wheel_step;
            
            const auto pre_scale_view_matrix = CalculateViewMatrix(persistent_scale_factor);
            const auto pre_scale_mouse_pos = MouseViewSpacePos(current_mouse_pos, pre_scale_view_matrix);

            persistent_scale_factor *= std::exp(scale);

            const auto post_scale_view_matrix = CalculateViewMatrix(persistent_scale_factor);
            const auto post_scale_mouse_pos = MouseViewSpacePos(current_mouse_pos, post_scale_view_matrix);

            const glm::vec3 view_space_correction = post_scale_mouse_pos - pre_scale_mouse_pos;
            translate_post_scaled += view_space_correction;
        }

        auto view_matrix = CalculateViewMatrix(persistent_scale_factor);
        mvp.SetView(view_matrix);

        persistent_mouse_pos = current_mouse_pos;
    }

private:

    glm::mat4 CalculateViewMatrix(float scale_factor)
    {
        auto pre_scaled = glm::translate(glm::mat4(1.f), translate_pre_scaled);
        auto post_scaled = glm::scale(pre_scaled, glm::vec3(scale_factor, scale_factor, 1.f));
        auto view_matrix = glm::translate(post_scaled, translate_post_scaled);
        return view_matrix;
    }

    glm::vec3 MouseClipSpace(const wxPoint& mouse_pos_px)
    {
        const glm::vec2 window_mouse_pos(static_cast<float>(mouse_pos_px.x), static_cast<float>(mouse_pos_px.y));

        std::array<GLint, 4> viewport_px;
        glGetIntegerv(GL_VIEWPORT, viewport_px.data());
        glm::vec4 viewport(0.f, 0.f, static_cast<float>(viewport_px[2]), static_cast<float>(viewport_px[3]));

        auto viewport_ortho = glm::ortho(viewport.x, viewport.z, viewport.w, viewport.y);
        auto mouse_pos_clip_space = viewport_ortho * glm::vec4(window_mouse_pos, 0.f, 1.f);
        return mouse_pos_clip_space;
    }

    glm::vec3 MouseWorldSpacePos(const wxPoint& mouse_pos_px, const MVP& mvp)
    {
        const glm::mat4 projection_matrix = mvp.GetProjection();
        const auto inverse_projection_matrix = glm::inverse(projection_matrix);

        const auto mouse_pos = inverse_projection_matrix * glm::vec4(MouseClipSpace(mouse_pos_px), 1.f);
        return glm::vec3(mouse_pos.x, mouse_pos.y, 0.f);
    }

    glm::vec3 MouseViewSpacePos(const glm::vec3& mouse_world_space_pos, const glm::mat4& view_matrix)
    {
        const auto inverse_view_matrix = glm::inverse(view_matrix);

        const auto mouse_pos = inverse_view_matrix * glm::vec4(mouse_world_space_pos, 1.f);
        return glm::vec3(mouse_pos.x, mouse_pos.y, 0.f);
    }

    float persistent_scale_factor;
    glm::vec3 persistent_mouse_pos;
    glm::vec3 translate_pre_scaled;
    glm::vec3 translate_post_scaled; 
};


class GLCanvas : public wxGLCanvas
{
public:

    GLCanvas(wxWindow* parent, const wxGLAttributes& canvas_attributes) :
        wxGLCanvas(parent, canvas_attributes),
        openGL_loaded(0)
    {}

    GraphicsEngine* GetGraphicsEngine()
    {
        return graphics_engine.get();
    }

    void LoadOpenGL(const std::array<int, 2>& version)
    {
        bool canvas_shown_on_screen = this->IsShownOnScreen();

        if (!canvas_shown_on_screen)
        {
            std::cerr << "Try wxFrame::Raise after WxFrame::Show" << '\n';
        }

        if (openGL_loaded == 0 && canvas_shown_on_screen)
        {
            context_attributes.PlatformDefaults().CoreProfile().OGLVersion(version[0], version[1]).EndList();
            context = std::make_unique<wxGLContext>(this, nullptr, &context_attributes);
            std::cout << "context IsOK " << context->IsOK() << '\n';

            SetCurrent(*context);
            openGL_loaded = gladLoadGL();

            std::cout << "OpenGL loaded: " << openGL_loaded << " version: " << GetGLVersionString() << '\n';

            glEnable(GL_CULL_FACE);
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LEQUAL);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_MULTISAMPLE);

            GLint sample_buffers = 0;
            glGetIntegerv(GL_SAMPLES, &sample_buffers);
            std::cout << "msaa sample_buffers " << sample_buffers << '\n';

            graphics_engine = std::make_unique<GraphicsEngine>();

            Bind(wxEVT_SIZE, &GLCanvas::SizeCallback, this);
            Bind(wxEVT_PAINT, &GLCanvas::PaintCallback, this);

            mouse_interaction = std::make_unique<MouseInteraction>();

            Bind(wxEVT_MOTION, &GLCanvas::MouseCallback, this);
            Bind(wxEVT_LEFT_DOWN, &GLCanvas::MouseCallback, this);
            Bind(wxEVT_LEFT_UP, &GLCanvas::MouseCallback, this);
            Bind(wxEVT_MOUSEWHEEL, &GLCanvas::MouseCallback, this);

            signal_opengl_loaded();
        }
    }

    static std::string GetGLVersionString()
    {
        std::string version_string;
        version_string += "GL_VERSION " + wxString::FromUTF8(reinterpret_cast<const char*>(glGetString(GL_VERSION))) + '\n';
        version_string += "GL_VENDOR " + wxString::FromUTF8(reinterpret_cast<const char*>(glGetString(GL_VENDOR))) + '\n';
        version_string += "GL_RENDERER " + wxString::FromUTF8(reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
        return version_string;
    }

    void ReceivePageSetup(const PageSetupConfig& page_setup_config)
    {
        page_size = rectf::from_dimension(page_setup_config.size[0], page_setup_config.size[1]);    
    }

    void RefreshMVP()
    {
        const float view_size_scale = 1.1f;

        wxSize size = GetClientSize();
        glViewport(0, 0, size.GetWidth(), size.GetHeight());

        rectf view_size = page_size.scale(view_size_scale);
        mvp.SetProjection(Projection::OrthoMatrix(view_size));
        
        graphics_engine->SetMVP(mvp);

        Refresh(false); //??
    }

    void SavePNG(std::string file_path)
    {
        RenderToPNG render_to_png(file_path, page_size, 600, graphics_engine);
    }

    sigslot::signal<> signal_opengl_loaded;

private:

    void PaintCallback(wxPaintEvent& event)
    {
        wxPaintDC dc(this);
        graphics_engine->SetMVP(mvp);
        graphics_engine->Render();
        SwapBuffers();
    }

    void SizeCallback(wxSizeEvent& event)
    {
        RefreshMVP();
        Refresh(false); //??
    }

    void MouseCallback(wxMouseEvent& event)
    {
        mouse_interaction->Interaction(mvp, event.GetPosition(), event.Dragging(), event.GetWheelRotation());
        RefreshMVP();
    }

    int openGL_loaded;

    wxGLContextAttrs context_attributes;
    std::unique_ptr<wxGLContext> context;
    
    std::shared_ptr<GraphicsEngine> graphics_engine;
    std::unique_ptr<MouseInteraction> mouse_interaction;

    rectf page_size;
    MVP mvp;
};

/*constexpr wxGLContextAttrs GLCanvas::defaultAttrs()
{
    /home/titan99/code/decade/src/gui/gl_canvas.cpp: In Konstruktor �GLCanvas::GLCanvas(wxWindow*, const wxGLAttributes&)�:
    /home/titan99/code/decade/src/gui/gl_canvas.cpp:24:42: Fehler: Adresse eines rvalues wird ermittelt [-fpermissive]
    context(this, nullptr, &defaultAttrs()),
    context_attributes.PlatformDefaults().CoreProfile().OGLVersion(3, 2).EndList();
    return context_attributes;
}*/
