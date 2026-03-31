#include <Client/Render/Renderer.hh>

#include <Helpers/Macros.hh>
#include <Helpers/Math.hh>
#include <Helpers/UTF8.hh>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <emscripten.h>
#include <chrono>

std::vector<Renderer *> Renderer::renderers;
double timer_1 = 0;
double time_wait = 0;

RenderContext::RenderContext() {}

RenderContext::RenderContext(Renderer *r) {
    *this = r->context;
    renderer = r;
EM_ASM({
    Module.ctxs[$0].save();
}, r->id);
    //reset();
}

void RenderContext::reset() {
    amount = 0;
    color_filter = 0;
    // Don't reset glitch_intensity - it's a persistent setting, not per-frame state
    // glitch_intensity is preserved
    clip_x = renderer->width / 2;
    clip_y = renderer->height / 2;
    //prevents premature unrendering
    clip_w = std::fmax(renderer->width, 10000.0);
    clip_h = std::fmax(renderer->height, 10000.0);
}

RenderContext::~RenderContext() {
    renderer->context = *this;
    EM_ASM({
        Module.ctxs[$0].restore();
    }, renderer->id);
}

Renderer::Renderer() : context() {
    id = EM_ASM_INT({
        let idx;
        if (Module.availableCtxs.length > 0)
            idx = Module.availableCtxs.pop();
        else
            idx = Module.ctxs.length;
        if (idx === 0) {
            Module.ctxs[idx] = document.getElementById('canvas').getContext('2d');
        } else {
            const canvas = new OffscreenCanvas(1,1);
            Module.ctxs[idx] = canvas.getContext('2d');
        }
        return idx;
    });
    DEBUG_ONLY(std::cout << "created canvas " << id << '\n';)
    Renderer::renderers.push_back(this);
    context.renderer = this;
    context.reset();
}

Renderer::~Renderer() {
    EM_ASM({
        if ($0 == 0)
            throw new Error('Tried to delete the main context');
        Module.ctxs[$0] = null;
        Module.availableCtxs.push($0);
    }, id);
    DEBUG_ONLY(std::cout << "removed canvas " << id << '\n';)
}

uint32_t Renderer::HSV(uint32_t c, float v) {
    return MIX(c >> 24 << 24, c, v);
}

uint32_t Renderer::MIX(uint32_t base, uint32_t mix, float v) {
    uint8_t b = fclamp((mix & 255) * v + (base & 255) * (1 - v), 0, 255);
    uint8_t g = fclamp(((mix >> 8) & 255) * v + ((base >> 8) & 255) * (1 - v), 0, 255);
    uint8_t r = fclamp(((mix >> 16) & 255) * v + ((base >> 16) & 255) * (1 - v), 0, 255);
    uint8_t a = base >> 24;
    return (a << 24) | (r << 16) | (g << 8) | b;
}

void Renderer::reset() {
    reset_transform();
    round_line_cap();
    round_line_join();
    center_text_align();
    center_text_baseline();
    context.reset();
}

void Renderer::set_dimensions(float w, float h) {
    width = w;
    height = h;
    EM_ASM({
        Module.ctxs[$0].canvas.width = $1;
        Module.ctxs[$0].canvas.height = $2;
    }, id, w, h);
}

void Renderer::add_color_filter(uint32_t c, float v) {
    context.color_filter = c;
    context.amount = v;
}

void Renderer::set_glitch_intensity(float intensity) {
    context.glitch_intensity = fclamp(intensity, 0.0f, 1.0f);
}

static double last_glitch_time = 0;

void Renderer::apply_glitch_filter() {
    if (context.glitch_intensity <= 0.0f) return;
    
    // Debug: ensure intensity is set
    float intensity = context.glitch_intensity;
    if (intensity <= 0.0f) return;
    
    // Rate limit glitch effects to prevent excessive memory usage
    double current_time = emscripten_get_now();
    double time_since_last_glitch = current_time - last_glitch_time;
    double min_interval = 200.0; // Minimum 200ms between glitch effects
    
    if (time_since_last_glitch < min_interval) {
        return;
    }
    
    last_glitch_time = current_time;
    
    // Always apply glitch when called (with rate limiting)
    
    EM_ASM({
        const ctx = Module.ctxs[$0];
        const canvas = ctx.canvas;
        const width = canvas.width;
        const height = canvas.height;
        const intensity = $1;
        
        if (intensity <= 0 || width <= 0 || height <= 0) return;
        
        // Always apply scanline displacement - this is the most visible effect
        const numScanlines = Math.max(8, Math.floor(intensity * 25));
        for (let i = 0; i < numScanlines; i++) {
            const y = Math.floor(Math.random() * height);
            const shift = Math.floor((Math.random() - 0.5) * intensity * 60);
            if (Math.abs(shift) < 4) continue;
            
            const sliceHeight = Math.max(3, Math.floor(Math.random() * 5 + 3));
            const tempCanvas = new OffscreenCanvas(width, sliceHeight);
            const tempCtx = tempCanvas.getContext('2d');
            
            tempCtx.drawImage(canvas, 0, y, width, sliceHeight, 0, 0, width, sliceHeight);
            ctx.drawImage(tempCanvas, shift, y);
        }
        
        // Horizontal frame shift - always apply for visibility (bigger shifts)
        const fullShift = Math.floor((Math.random() - 0.5) * intensity * 40);
        if (Math.abs(fullShift) > 2) {
            const tempCanvas = new OffscreenCanvas(width, height);
            const tempCtx = tempCanvas.getContext('2d');
            tempCtx.drawImage(canvas, 0, 0);
            ctx.clearRect(0, 0, width, height);
            ctx.drawImage(tempCanvas, fullShift, 0);
        }
        
        // Noise effect - always apply for maximum visibility
        ctx.save();
        ctx.globalCompositeOperation = 'difference';
        const noiseCount = Math.floor(intensity * 2400);
        for (let i = 0; i < noiseCount; i++) {
            const x = Math.floor(Math.random() * width);
            const y = Math.floor(Math.random() * height);
            const r = Math.floor(Math.random() * 255);
            const g = Math.floor(Math.random() * 255);
            const b = Math.floor(Math.random() * 255);
            ctx.fillStyle = `rgba(${r}, ${g}, ${b}, ${intensity * 0.4})`;
            ctx.fillRect(x, y, 3, 3);
        }
        ctx.restore();
    }, id, intensity);
}

void Renderer::set_fill(uint32_t v) {
    v = MIX(v, context.color_filter, context.amount);
    if (emscripten_get_now() - timer_1 < time_wait || !do_random_colors){
        EM_ASM({
        Module.ctxs[$0].fillStyle = "rgba("+$3+","+$2+","+$1+","+$4/255+")";
        }, id, v & 255, (v >> 8) & 255, (v >> 16) & 255, v >> 24);
    }
    else if (emscripten_get_now() - timer_1 > (time_wait + 1000)){
        timer_1 = emscripten_get_now();
        time_wait = ((double)std::rand() / RAND_MAX) * 10000.0;
        printf("time_wait: %f\n", time_wait);
    }
    else {
        EM_ASM({
        console.log("random color");
        const r = Math.floor(Math.random() * 256);
        const g = Math.floor(Math.random() * 256);
        const b = Math.floor(Math.random() * 256);
        Module.ctxs[$0].fillStyle = "rgb("+r+","+g+","+b+")";
        }, id);
    }
}

void Renderer::set_stroke(uint32_t v) {
    v = MIX(v, context.color_filter, context.amount);
    EM_ASM({
    Module.ctxs[$0].strokeStyle = "rgba("+$3+","+$2+","+$1+","+$4/255+")";
    }, id, v & 255, (v >> 8) & 255, (v >> 16) & 255, v >> 24);
}

void Renderer::set_line_width(float v) {
    EM_ASM({
    Module.ctxs[$0].lineWidth = $1;
    }, id, v);
}

void Renderer::set_text_size(float v) {
    EM_ASM({
    Module.ctxs[$0].font = $1 + "px Ubuntu";
    }, id, v);
}

void Renderer::set_global_alpha(float v) {
    EM_ASM({
    Module.ctxs[$0].globalAlpha = $1;
    }, id, v);
}

void Renderer::round_line_cap() {
    EM_ASM({
    Module.ctxs[$0].lineCap = "round";
    }, id);
}

void Renderer::round_line_join() {
    EM_ASM({
    Module.ctxs[$0].lineJoin = "round";
    }, id);
}

void Renderer::center_text_align() {
    EM_ASM({
    Module.ctxs[$0].textAlign = "center";
    }, id);
}

void Renderer::center_text_baseline() {
    EM_ASM({
    Module.ctxs[$0].textBaseline = "middle";
    }, id);
}

static void update_transform(Renderer *r) {
EM_ASM({
    Module.ctxs[$0].setTransform($1, $2, $4, $5, $3, $6);
}, r->id, r->context.transform_matrix[0],r->context.transform_matrix[1],r->context.transform_matrix[2], 
r->context.transform_matrix[3],r->context.transform_matrix[4],r->context.transform_matrix[5]);
}

void Renderer::set_transform(float a, float b, float c, float d, float e, float f) {
    context.transform_matrix[0] = a;
    context.transform_matrix[1] = b;
    context.transform_matrix[2] = c;
    context.transform_matrix[3] = d;
    context.transform_matrix[4] = e;
    context.transform_matrix[5] = f;
    update_transform(this);
}

void Renderer::scale(float v) {
    context.transform_matrix[0] *= v;
    context.transform_matrix[1] *= v;
    context.transform_matrix[3] *= v;
    context.transform_matrix[4] *= v;
    update_transform(this);
}

void Renderer::scale(float x, float y) {
    context.transform_matrix[0] *= x;
    context.transform_matrix[1] *= x;
    context.transform_matrix[3] *= y;
    context.transform_matrix[4] *= y;
    update_transform(this);
}

void Renderer::translate(float x, float y) {
    context.transform_matrix[2] += x * context.transform_matrix[0] + y * context.transform_matrix[3];
    context.transform_matrix[5] += y * context.transform_matrix[4] + x * context.transform_matrix[1];
    update_transform(this);
}

void Renderer::rotate(float a) {
    float cos_a = cosf(a);
    float sin_a = sinf(a);
    float original0 = context.transform_matrix[0];
    float original1 = context.transform_matrix[1];
    float original3 = context.transform_matrix[3];
    float original4 = context.transform_matrix[4];
    context.transform_matrix[0] = original0 * cos_a + original1 * -sin_a;
    context.transform_matrix[1] = original0 * sin_a + original1 * cos_a;
    context.transform_matrix[3] = original3 * cos_a + original4 * -sin_a;
    context.transform_matrix[4] = original3 * sin_a + original4 * cos_a;
    update_transform(this);
}

void Renderer::reset_transform() {
    set_transform(1,0,0,0,1,0);
}

void Renderer::begin_path() {
    EM_ASM({
        Module.ctxs[$0].beginPath();
    }, id);
}

void Renderer::move_to(float x, float y) {
    EM_ASM({
        Module.ctxs[$0].moveTo($1, $2);
    }, id, x, y);
    }

void Renderer::line_to(float x, float y) {
    EM_ASM({
        Module.ctxs[$0].lineTo($1, $2);
    }, id, x, y);
}

void Renderer::qcurve_to(float x, float y, float x1, float y1) {
    EM_ASM({
        Module.ctxs[$0].quadraticCurveTo($1, $2, $3, $4);
    }, id, x, y, x1, y1);
}

void Renderer::bcurve_to(float x, float y, float x1, float y1, float x2, float y2) {
    EM_ASM({
        Module.ctxs[$0].bezierCurveTo($1, $2, $3, $4, $5, $6);
    }, id, x, y, x1, y1, x2, y2);
}


void Renderer::partial_arc(float x, float y, float r, float sa, float ea, uint8_t ccw) {
    EM_ASM({
        Module.ctxs[$0].arc($1, $2, $3, $4, $5, !!$6);
    }, id, x, y, r, sa, ea, ccw);
}

void Renderer::arc(float x, float y, float r) {
    partial_arc(x, y, r, 0, 2 * M_PI, 0);
}

void Renderer::reverse_arc(float x, float y, float r) {
    partial_arc(x, y, r, 0, 2 * M_PI, 1);
}

void Renderer::ellipse(float x, float y, float r1, float r2, float a) {
    EM_ASM({
        Module.ctxs[$0].ellipse($1, $2, $3, $4, $5, 2 * Math.PI, 0);
    }, id, x, y, r1, r2, a);
}

void Renderer::ellipse(float x, float y, float r1, float r2) {
    ellipse(x, y, r1, r2, 0);
}

void Renderer::fill_rect(float x, float y, float w, float h) {
    EM_ASM({
        Module.ctxs[$0].fillRect($1, $2, $3, $4);
    }, id, x, y, w, h);
}

void Renderer::stroke_rect(float x, float y, float w, float h) {
    EM_ASM({
        Module.ctxs[$0].strokeRect($1, $2, $3, $4);
    }, id, x, y, w, h);
}

void Renderer::rect(float x, float y, float w, float h) {
    EM_ASM({
        Module.ctxs[$0].rect($1, $2, $3, $4);
    }, id, x, y, w, h);
}

void Renderer::round_rect(float x, float y, float w, float h, float r) {
    move_to(x + r, y);
    line_to(x + w - r, y);
    qcurve_to(x + w, y, x + w, y + r);
    line_to(x + w, y + h - r);
    qcurve_to(x + w, y + h, x + w - r, y + h);
    line_to(x + r, y + h);
    qcurve_to(x, y + h, x, y + h - r);
    line_to(x, y + r);
    qcurve_to(x, y, x + r, y);
}

void Renderer::close_path() {
    EM_ASM({
        Module.ctxs[$0].closePath();
    }, id);
}

void Renderer::fill(uint8_t o) {
    EM_ASM({
        Module.ctxs[$0].fill($1 ? "nonzero" : "evenodd");
    }, id, o);
}

void Renderer::stroke() {
    EM_ASM({
        Module.ctxs[$0].stroke();
    }, id);
}

void Renderer::clip() {
    EM_ASM({
        Module.ctxs[$0].clip();
    }, id);
}

void Renderer::clip_rect(float x, float y, float w, float h) {
    //assumes axis oriented scaling
    context.clip_x = x * context.transform_matrix[0] + context.transform_matrix[2];
    context.clip_w = w * context.transform_matrix[0];
    context.clip_y = y * context.transform_matrix[4] + context.transform_matrix[5];
    context.clip_h = h * context.transform_matrix[4];
    begin_path();
    rect(x-w/2,y-h/2,w,h);
    clip();
}

void Renderer::draw_image(Renderer &ctx) {
    EM_ASM({
        Module.ctxs[$0].drawImage(Module.ctxs[$1].canvas, $2, $3);
    }, id, ctx.id, -ctx.width / 2, -ctx.height / 2);
}

void Renderer::fill_text(char const *text) {
    EM_ASM({
        Module.ctxs[$0].fillText(Module.TextDecoder.decode(HEAPU8.subarray($1, $1+$2)),0,0);
    }, id, text, std::strlen(text));
}

void Renderer::stroke_text(char const *text) {
    EM_ASM({
        Module.ctxs[$0].strokeText(Module.TextDecoder.decode(HEAPU8.subarray($1, $1+$2)),0,0);
    }, id, text, std::strlen(text));
}

void Renderer::draw_text(char const *text, struct TextArgs const args) {
    set_fill(args.fill);
    set_stroke(args.stroke);
    set_text_size(args.size);
    if (args.stroke_scale > 0) {
        set_line_width(args.size * args.stroke_scale);
        stroke_text(text);
    }
    fill_text(text);
}

float Renderer::get_text_size(char const *text) {
    return EM_ASM_DOUBLE({
        return Module.ctxs[$0].measureText(Module.TextDecoder.decode(HEAPU8.subarray($1, $1+$2)),0,0).width;
    }, id, text, std::strlen(text));
}

//precalculated ascii for standard Ubuntu font, only serves as an approximation (will fail for certain scripts)
constexpr float CHAR_WIDTHS[128] = {0,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0,0.24,0.24,0.24,0.24,0.24,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0,0.5,0.5,0.24,0.286,0.465,0.699,0.568,0.918,0.705,0.247,0.356,0.356,0.502,0.568,0.246,0.34,0.246,0.437,0.568,0.568,0.568,0.568,0.568,0.568,0.568,0.568,0.568,0.568,0.246,0.246,0.568,0.568,0.568,0.455,0.974,0.721,0.672,0.648,0.737,0.606,0.574,0.702,0.734,0.316,0.529,0.684,0.563,0.897,0.756,0.79,0.644,0.79,0.667,0.582,0.614,0.707,0.722,0.948,0.675,0.661,0.61,0.371,0.437,0.371,0.568,0.5,0.286,0.553,0.604,0.5,0.604,0.584,0.422,0.594,0.589,0.289,0.289,0.579,0.316,0.862,0.589,0.607,0.604,0.604,0.422,0.485,0.444,0.589,0.55,0.784,0.554,0.547,0.5,0.371,0.322,0.371,0.568,0.5};

float Renderer::get_ascii_text_size(char const *text) {
    float w = 0;
    UTF8Parser parser(text);
    while (1) {
        uint32_t c = parser.next_symbol();
        if (c == 0) break;
        if (c < 128)
            w += CHAR_WIDTHS[c];
        else
            w += 1;
    }
    return w;
}