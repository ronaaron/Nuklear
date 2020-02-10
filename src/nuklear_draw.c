#include "nuklear.h"
#include "nuklear_internal.h"

/* ==============================================================
 *
 *                          DRAW
 *
 * ===============================================================*/
NK_LIB void
nk_command_buffer_init(struct nk_command_buffer *cb,
    struct nk_buffer *b, enum nk_command_clipping clip)
{
    NK_ASSERT(cb);
    NK_ASSERT(b);
    if (!cb || !b) return;
    cb->base = b;
    cb->use_clipping = (int)clip;
    cb->begin = b->allocated;
    cb->end = b->allocated;
    cb->last = b->allocated;
}
NK_LIB void
nk_command_buffer_reset(struct nk_command_buffer *b)
{
    NK_ASSERT(b);
    if (!b) return;
    b->begin = 0;
    b->end = 0;
    b->last = 0;
    b->clip = nk_null_rect;
#ifdef NK_INCLUDE_COMMAND_USERDATA
    b->userdata.ptr = 0;
#endif
}
NK_LIB void*
nk_command_buffer_push(struct nk_command_buffer* b,
    enum nk_command_type t, nk_size size)
{
    NK_STORAGE const nk_size align = NK_ALIGNOF(struct nk_command);
    struct nk_command *cmd;
    nk_size alignment;
    void *unaligned;
    void *memory;

    NK_ASSERT(b);
    NK_ASSERT(b->base);
    if (!b) return 0;
    cmd = (struct nk_command*)nk_buffer_alloc(b->base,NK_BUFFER_FRONT,size,align);
    if (!cmd) return 0;

    /* make sure the offset to the next command is aligned */
    b->last = (nk_size)((nk_byte*)cmd - (nk_byte*)b->base->memory.ptr);
    unaligned = (nk_byte*)cmd + size;
    memory = NK_ALIGN_PTR(unaligned, align);
    alignment = (nk_size)((nk_byte*)memory - (nk_byte*)unaligned);
#ifdef NK_ZERO_COMMAND_MEMORY
    NK_MEMSET(cmd, 0, size + alignment);
#endif

    cmd->type = t;
    cmd->next = b->base->allocated + alignment;
#ifdef NK_INCLUDE_COMMAND_USERDATA
    cmd->userdata = b->userdata;
#endif
    b->end = cmd->next;
    return cmd;
}
NK_API void
nk_push_scissor(struct nk_command_buffer *b, struct nk_rect r)
{
    struct nk_command_scissor *cmd;
    NK_ASSERT(b);
    if (!b) return;

    b->clip.x = r.x;
    b->clip.y = r.y;
    b->clip.w = r.w;
    b->clip.h = r.h;
    cmd = (struct nk_command_scissor*)
        nk_command_buffer_push(b, NK_COMMAND_SCISSOR, sizeof(*cmd));

    if (!cmd) return;
    cmd->x = (short)r.x;
    cmd->y = (short)r.y;
    cmd->w = (unsigned short)NK_MAX(0, r.w);
    cmd->h = (unsigned short)NK_MAX(0, r.h);
}
NK_API void
nk_stroke_line(struct nk_command_buffer *b, float x0, float y0,
    float x1, float y1, float line_thickness, struct nk_color c)
{
    struct nk_command_line *cmd;
    NK_ASSERT(b);
    if (!b || line_thickness <= 0) return;
#ifdef NK_INCLUDE_AFFINE_TRANSFORM
	if (b->transform_active && *b->transform_active && b->transform)
	{
		nk_apply_transform(b->transform, &x0, &y0);
		nk_apply_transform(b->transform, &x1, &y1);
	}
#endif
    cmd = (struct nk_command_line*)
        nk_command_buffer_push(b, NK_COMMAND_LINE, sizeof(*cmd));
    if (!cmd) return;
    cmd->line_thickness = (unsigned short)line_thickness;
    cmd->begin.x = (short)x0;
    cmd->begin.y = (short)y0;
    cmd->end.x = (short)x1;
    cmd->end.y = (short)y1;
    cmd->color = c;
}
NK_API void
nk_stroke_curve(struct nk_command_buffer *b, float ax, float ay,
    float ctrl0x, float ctrl0y, float ctrl1x, float ctrl1y,
    float bx, float by, float line_thickness, struct nk_color col)
{
    struct nk_command_curve *cmd;
    NK_ASSERT(b);
    if (!b || col.a == 0 || line_thickness <= 0) return;

    cmd = (struct nk_command_curve*)
        nk_command_buffer_push(b, NK_COMMAND_CURVE, sizeof(*cmd));
    if (!cmd) return;
    cmd->line_thickness = (unsigned short)line_thickness;
    cmd->begin.x = (short)ax;
    cmd->begin.y = (short)ay;
    cmd->ctrl[0].x = (short)ctrl0x;
    cmd->ctrl[0].y = (short)ctrl0y;
    cmd->ctrl[1].x = (short)ctrl1x;
    cmd->ctrl[1].y = (short)ctrl1y;
    cmd->end.x = (short)bx;
    cmd->end.y = (short)by;
    cmd->color = col;
}
NK_API void
nk_stroke_rect(struct nk_command_buffer *b, struct nk_rect rect,
    float rounding, float line_thickness, struct nk_color c)
{
    struct nk_command_rect *cmd;
    NK_ASSERT(b);
#ifdef NK_INCLUDE_AFFINE_TRANSFORM
	if (b->transform_active && *b->transform_active && b->transform)
	{
		float x2 = rect.x + rect.w;
		float y2 = rect.y + rect.h;
		nk_apply_transform(b->transform, &rect.x, &rect.y);
		nk_apply_transform(b->transform, &x2, &y2);
		rect.w = x2-rect.x;
		rect.h = y2-rect.y;

	}
#endif
    if (!b || c.a == 0 || rect.w == 0 || rect.h == 0 || line_thickness <= 0) return;
    if (b->use_clipping) {
        const struct nk_rect *clip = &b->clip;
        if (!NK_INTERSECT(rect.x, rect.y, rect.w, rect.h,
            clip->x, clip->y, clip->w, clip->h)) return;
    }
    cmd = (struct nk_command_rect*)
        nk_command_buffer_push(b, NK_COMMAND_RECT, sizeof(*cmd));
    if (!cmd) return;
    cmd->rounding = (unsigned short)rounding;
    cmd->line_thickness = (unsigned short)line_thickness;
    cmd->x = (short)rect.x;
    cmd->y = (short)rect.y;
    cmd->w = (unsigned short)NK_MAX(0, rect.w);
    cmd->h = (unsigned short)NK_MAX(0, rect.h);
    cmd->color = c;
}
NK_API void
nk_fill_rect(struct nk_command_buffer *b, struct nk_rect rect,
    float rounding, struct nk_color c)
{
    struct nk_command_rect_filled *cmd;
    NK_ASSERT(b);
#ifdef NK_INCLUDE_AFFINE_TRANSFORM
	if (b->transform_active && *b->transform_active && b->transform)
	{
		float x2 = rect.x + rect.w;
		float y2 = rect.y + rect.h;
		nk_apply_transform(b->transform, &rect.x, &rect.y);
		nk_apply_transform(b->transform, &x2, &y2);
		rect.w = x2-rect.x;
		rect.h = y2-rect.y;

	}
#endif
    if (!b || c.a == 0 || rect.w == 0 || rect.h == 0) return;
    if (b->use_clipping) {
        const struct nk_rect *clip = &b->clip;
        if (!NK_INTERSECT(rect.x, rect.y, rect.w, rect.h,
            clip->x, clip->y, clip->w, clip->h)) return;
    }

    cmd = (struct nk_command_rect_filled*)
        nk_command_buffer_push(b, NK_COMMAND_RECT_FILLED, sizeof(*cmd));
    if (!cmd) return;
    cmd->rounding = (unsigned short)rounding;
    cmd->x = (short)rect.x;
    cmd->y = (short)rect.y;
    cmd->w = (unsigned short)NK_MAX(0, rect.w);
    cmd->h = (unsigned short)NK_MAX(0, rect.h);
    cmd->color = c;
}
NK_API void
nk_fill_rect_multi_color(struct nk_command_buffer *b, struct nk_rect rect,
    struct nk_color left, struct nk_color top, struct nk_color right,
    struct nk_color bottom)
{
    struct nk_command_rect_multi_color *cmd;
    NK_ASSERT(b);
#ifdef NK_INCLUDE_AFFINE_TRANSFORM
	if (b->transform_active && *b->transform_active && b->transform)
	{
		float x2 = rect.x + rect.w;
		float y2 = rect.y + rect.h;
		nk_apply_transform(b->transform, &rect.x, &rect.y);
		nk_apply_transform(b->transform, &x2, &y2);
		rect.w = x2-rect.x;
		rect.h = y2-rect.y;

	}
#endif
    if (!b || rect.w == 0 || rect.h == 0) return;
    if (b->use_clipping) {
        const struct nk_rect *clip = &b->clip;
        if (!NK_INTERSECT(rect.x, rect.y, rect.w, rect.h,
            clip->x, clip->y, clip->w, clip->h)) return;
    }

    cmd = (struct nk_command_rect_multi_color*)
        nk_command_buffer_push(b, NK_COMMAND_RECT_MULTI_COLOR, sizeof(*cmd));
    if (!cmd) return;
    cmd->x = (short)rect.x;
    cmd->y = (short)rect.y;
    cmd->w = (unsigned short)NK_MAX(0, rect.w);
    cmd->h = (unsigned short)NK_MAX(0, rect.h);
    cmd->left = left;
    cmd->top = top;
    cmd->right = right;
    cmd->bottom = bottom;
}
NK_API void
nk_stroke_circle(struct nk_command_buffer *b, struct nk_rect r,
    float line_thickness, struct nk_color c)
{
    struct nk_command_circle *cmd;
    if (!b || r.w == 0 || r.h == 0 || line_thickness <= 0) return;
#ifdef NK_INCLUDE_AFFINE_TRANSFORM
	if (b->transform_active && *b->transform_active && b->transform)
	{
		float x2 = r.x + r.w;
		float y2 = r.y + r.h;
		nk_apply_transform(b->transform, &r.x, &r.y);
		nk_apply_transform(b->transform, &x2, &y2);
		r.w = x2-r.x;
		r.h = y2-r.y;

	}
#endif
    if (b->use_clipping) {
        const struct nk_rect *clip = &b->clip;
        if (!NK_INTERSECT(r.x, r.y, r.w, r.h, clip->x, clip->y, clip->w, clip->h))
            return;
    }

    cmd = (struct nk_command_circle*)
        nk_command_buffer_push(b, NK_COMMAND_CIRCLE, sizeof(*cmd));
    if (!cmd) return;
    cmd->line_thickness = (unsigned short)line_thickness;
    cmd->x = (short)r.x;
    cmd->y = (short)r.y;
    cmd->w = (unsigned short)NK_MAX(r.w, 0);
    cmd->h = (unsigned short)NK_MAX(r.h, 0);
    cmd->color = c;
}
NK_API void
nk_fill_circle(struct nk_command_buffer *b, struct nk_rect r, struct nk_color c)
{
    struct nk_command_circle_filled *cmd;
    NK_ASSERT(b);
    if (!b || c.a == 0 || r.w == 0 || r.h == 0) return;
#ifdef NK_INCLUDE_AFFINE_TRANSFORM
	if (b->transform_active && *b->transform_active && b->transform)
	{
		float x2 = r.x + r.w;
		float y2 = r.y + r.h;
		nk_apply_transform(b->transform, &r.x, &r.y);
		nk_apply_transform(b->transform, &x2, &y2);
		r.w = x2-r.x;
		r.h = y2-r.y;

	}
#endif
    if (b->use_clipping) {
        const struct nk_rect *clip = &b->clip;
        if (!NK_INTERSECT(r.x, r.y, r.w, r.h, clip->x, clip->y, clip->w, clip->h))
            return;
    }

    cmd = (struct nk_command_circle_filled*)
        nk_command_buffer_push(b, NK_COMMAND_CIRCLE_FILLED, sizeof(*cmd));
    if (!cmd) return;
    cmd->x = (short)r.x;
    cmd->y = (short)r.y;
    cmd->w = (unsigned short)NK_MAX(r.w, 0);
    cmd->h = (unsigned short)NK_MAX(r.h, 0);
    cmd->color = c;
}
NK_API void
nk_stroke_arc(struct nk_command_buffer *b, float cx, float cy, float radius,
    float a_min, float a_max, float line_thickness, struct nk_color c)
{
    struct nk_command_arc *cmd;
    if (!b || c.a == 0 || line_thickness <= 0) return;
#ifdef NK_INCLUDE_AFFINE_TRANSFORM
	if (b->transform_active && *b->transform_active && b->transform)
	{
		nk_apply_transform(b->transform, &cx, &cy);
	}
#endif
    cmd = (struct nk_command_arc*)
        nk_command_buffer_push(b, NK_COMMAND_ARC, sizeof(*cmd));
    if (!cmd) return;
    cmd->line_thickness = (unsigned short)line_thickness;
    cmd->cx = (short)cx;
    cmd->cy = (short)cy;
    cmd->r = (unsigned short)radius;
    cmd->a[0] = a_min;
    cmd->a[1] = a_max;
    cmd->color = c;
}
NK_API void
nk_fill_arc(struct nk_command_buffer *b, float cx, float cy, float radius,
    float a_min, float a_max, struct nk_color c)
{
    struct nk_command_arc_filled *cmd;
    NK_ASSERT(b);
    if (!b || c.a == 0) return;
#ifdef NK_INCLUDE_AFFINE_TRANSFORM
	if (b->transform_active && *b->transform_active && b->transform)
	{
		nk_apply_transform(b->transform, &cx, &cy);
	}
#endif
    cmd = (struct nk_command_arc_filled*)
        nk_command_buffer_push(b, NK_COMMAND_ARC_FILLED, sizeof(*cmd));
    if (!cmd) return;
    cmd->cx = (short)cx;
    cmd->cy = (short)cy;
    cmd->r = (unsigned short)radius;
    cmd->a[0] = a_min;
    cmd->a[1] = a_max;
    cmd->color = c;
}
NK_API void
nk_stroke_triangle(struct nk_command_buffer *b, float x0, float y0, float x1,
    float y1, float x2, float y2, float line_thickness, struct nk_color c)
{
    struct nk_command_triangle *cmd;
    NK_ASSERT(b);
    if (!b || c.a == 0 || line_thickness <= 0) return;
#ifdef NK_INCLUDE_AFFINE_TRANSFORM
	if (b->transform_active && *b->transform_active && b->transform)
	{
		nk_apply_transform(b->transform, &x0, &y0);
		nk_apply_transform(b->transform, &x1, &y1);
		nk_apply_transform(b->transform, &x2, &y2);
	}
#endif
    if (b->use_clipping) {
        const struct nk_rect *clip = &b->clip;
        if (!NK_INBOX(x0, y0, clip->x, clip->y, clip->w, clip->h) &&
            !NK_INBOX(x1, y1, clip->x, clip->y, clip->w, clip->h) &&
            !NK_INBOX(x2, y2, clip->x, clip->y, clip->w, clip->h))
            return;
    }

    cmd = (struct nk_command_triangle*)
        nk_command_buffer_push(b, NK_COMMAND_TRIANGLE, sizeof(*cmd));
    if (!cmd) return;
    cmd->line_thickness = (unsigned short)line_thickness;
    cmd->a.x = (short)x0;
    cmd->a.y = (short)y0;
    cmd->b.x = (short)x1;
    cmd->b.y = (short)y1;
    cmd->c.x = (short)x2;
    cmd->c.y = (short)y2;
    cmd->color = c;
}
NK_API void
nk_fill_triangle(struct nk_command_buffer *b, float x0, float y0, float x1,
    float y1, float x2, float y2, struct nk_color c)
{
    struct nk_command_triangle_filled *cmd;
    NK_ASSERT(b);
    if (!b || c.a == 0) return;
    if (!b) return;
#ifdef NK_INCLUDE_AFFINE_TRANSFORM
	if (b->transform_active && *b->transform_active && b->transform)
	{
		nk_apply_transform(b->transform, &x0, &y0);
		nk_apply_transform(b->transform, &x1, &y1);
		nk_apply_transform(b->transform, &x2, &y2);
	}
#endif
    if (b->use_clipping) {
        const struct nk_rect *clip = &b->clip;
        if (!NK_INBOX(x0, y0, clip->x, clip->y, clip->w, clip->h) &&
            !NK_INBOX(x1, y1, clip->x, clip->y, clip->w, clip->h) &&
            !NK_INBOX(x2, y2, clip->x, clip->y, clip->w, clip->h))
            return;
    }

    cmd = (struct nk_command_triangle_filled*)
        nk_command_buffer_push(b, NK_COMMAND_TRIANGLE_FILLED, sizeof(*cmd));
    if (!cmd) return;
    cmd->a.x = (short)x0;
    cmd->a.y = (short)y0;
    cmd->b.x = (short)x1;
    cmd->b.y = (short)y1;
    cmd->c.x = (short)x2;
    cmd->c.y = (short)y2;
    cmd->color = c;
}
NK_API void
nk_stroke_polygon(struct nk_command_buffer *b,  float *points, int point_count,
    float line_thickness, struct nk_color col)
{
    int i;
    nk_size size = 0;
    struct nk_command_polygon *cmd;

    NK_ASSERT(b);
    if (!b || col.a == 0 || line_thickness <= 0) return;
    size = sizeof(*cmd) + sizeof(short) * 2 * (nk_size)point_count;
    cmd = (struct nk_command_polygon*) nk_command_buffer_push(b, NK_COMMAND_POLYGON, size);
    if (!cmd) return;
    cmd->color = col;
    cmd->line_thickness = (unsigned short)line_thickness;
    cmd->point_count = (unsigned short)point_count;
    for (i = 0; i < point_count; ++i) {
        cmd->points[i].x = (short)points[i*2];
        cmd->points[i].y = (short)points[i*2+1];
    }
}
NK_API void
nk_fill_polygon(struct nk_command_buffer *b, float *points, int point_count,
    struct nk_color col)
{
    int i;
    nk_size size = 0;
    struct nk_command_polygon_filled *cmd;

    NK_ASSERT(b);
    if (!b || col.a == 0) return;
    size = sizeof(*cmd) + sizeof(short) * 2 * (nk_size)point_count;
    cmd = (struct nk_command_polygon_filled*)
        nk_command_buffer_push(b, NK_COMMAND_POLYGON_FILLED, size);
    if (!cmd) return;
    cmd->color = col;
    cmd->point_count = (unsigned short)point_count;
    for (i = 0; i < point_count; ++i) {
        cmd->points[i].x = (short)points[i*2+0];
        cmd->points[i].y = (short)points[i*2+1];
    }
}
NK_API void
nk_stroke_polyline(struct nk_command_buffer *b, float *points, int point_count,
    float line_thickness, struct nk_color col)
{
    int i;
    nk_size size = 0;
    struct nk_command_polyline *cmd;

    NK_ASSERT(b);
    if (!b || col.a == 0 || line_thickness <= 0) return;
    size = sizeof(*cmd) + sizeof(short) * 2 * (nk_size)point_count;
    cmd = (struct nk_command_polyline*) nk_command_buffer_push(b, NK_COMMAND_POLYLINE, size);
    if (!cmd) return;
    cmd->color = col;
    cmd->point_count = (unsigned short)point_count;
    cmd->line_thickness = (unsigned short)line_thickness;
    for (i = 0; i < point_count; ++i) {
        cmd->points[i].x = (short)points[i*2];
        cmd->points[i].y = (short)points[i*2+1];
    }
}
NK_API void
nk_draw_image(struct nk_command_buffer *b, struct nk_rect r,
    const struct nk_image *img, struct nk_color col)
{
    struct nk_command_image *cmd;
    NK_ASSERT(b);
    if (!b) return;
    if (b->use_clipping) {
        const struct nk_rect *c = &b->clip;
        if (c->w == 0 || c->h == 0 || !NK_INTERSECT(r.x, r.y, r.w, r.h, c->x, c->y, c->w, c->h))
            return;
    }

    cmd = (struct nk_command_image*)
        nk_command_buffer_push(b, NK_COMMAND_IMAGE, sizeof(*cmd));
    if (!cmd) return;
    cmd->x = (short)r.x;
    cmd->y = (short)r.y;
    cmd->w = (unsigned short)NK_MAX(0, r.w);
    cmd->h = (unsigned short)NK_MAX(0, r.h);
    cmd->img = *img;
    cmd->col = col;
}
NK_API void
nk_push_custom(struct nk_command_buffer *b, struct nk_rect r,
    nk_command_custom_callback cb, nk_handle usr)
{
    struct nk_command_custom *cmd;
    NK_ASSERT(b);
    if (!b) return;
    if (b->use_clipping) {
        const struct nk_rect *c = &b->clip;
        if (c->w == 0 || c->h == 0 || !NK_INTERSECT(r.x, r.y, r.w, r.h, c->x, c->y, c->w, c->h))
            return;
    }

    cmd = (struct nk_command_custom*)
        nk_command_buffer_push(b, NK_COMMAND_CUSTOM, sizeof(*cmd));
    if (!cmd) return;
    cmd->x = (short)r.x;
    cmd->y = (short)r.y;
    cmd->w = (unsigned short)NK_MAX(0, r.w);
    cmd->h = (unsigned short)NK_MAX(0, r.h);
    cmd->callback_data = usr;
    cmd->callback = cb;
}
NK_API void
nk_draw_text(struct nk_command_buffer *b, struct nk_rect r,
    const char *string, int length, const struct nk_user_font *font,
    struct nk_color bg, struct nk_color fg)
{
    float text_width = 0;
    struct nk_command_text *cmd;

    NK_ASSERT(b);
    NK_ASSERT(font);
    if (!b || !string || !length || (bg.a == 0 && fg.a == 0)) return;
    if (b->use_clipping) {
        const struct nk_rect *c = &b->clip;
        if (c->w == 0 || c->h == 0 || !NK_INTERSECT(r.x, r.y, r.w, r.h, c->x, c->y, c->w, c->h))
            return;
    }

    /* make sure text fits inside bounds */
    text_width = font->width(font->userdata, font->height, string, length);
    if (text_width > r.w){
        int glyphs = 0;
        float txt_width = (float)text_width;
        length = nk_text_clamp(font, string, length, r.w, &glyphs, &txt_width, 0,0);
    }

    if (!length) return;
    cmd = (struct nk_command_text*)
        nk_command_buffer_push(b, NK_COMMAND_TEXT, sizeof(*cmd) + (nk_size)(length + 1));
    if (!cmd) return;
    cmd->x = (short)r.x;
    cmd->y = (short)r.y;
    cmd->w = (unsigned short)r.w;
    cmd->h = (unsigned short)r.h;
    cmd->background = bg;
    cmd->foreground = fg;
    cmd->font = font;
    cmd->length = length;
    cmd->height = font->height;
    NK_MEMCPY(cmd->string, string, (nk_size)length);
    cmd->string[length] = '\0';
}
#ifdef NK_INCLUDE_AFFINE_TRANSFORM
void 
nk_apply_transform(const struct nk_affine_transform *t, float *x, float *y)
{
	NK_ASSERT(t);
	NK_ASSERT(x);
	NK_ASSERT(y);

	/*
	 * The transform is :
	 *    a b c
	 *    d e f
	 *    0 0 1
	 *
	 * where the last row is omitted
	 */
	float x1 = *x;
	float y1 = *y;
	float x2 = (t->a * x1) + (t->b * y1) + t->c;
	float y2 = (t->d * x1) + (t->e * y1) + t->f;
	*x = x2;
	*y = y2;
}

static void
nk_compose_transform(struct nk_affine_transform *out, const struct nk_affine_transform *in)
{
	struct nk_affine_transform result;
	/* 3X3 matrix multiplication, but we discard the bottom row */
	result.a = (out->a * in->a) + (out->b * in->d);
	result.b = (out->a * in->b) + (out->b * in->e);
	result.c = (out->a * in->c) + (out->b * in->f) + out->c;
	
	result.d = (out->d * in->a) + (out->e * in->d);
	result.e = (out->d * in->b) + (out->e * in->e);
	result.f = (out->d * in->c) + (out->e * in->f) + out->f;

	NK_MEMCPY(out, &result, sizeof(struct nk_affine_transform));
}

static void 
nk_set_transform(struct nk_context *ctx, const struct nk_affine_transform *t)
{
	if (ctx->transform_active)
	{
		nk_compose_transform(&ctx->transform, t);
	}
	else
	{
		NK_MEMCPY(&ctx->transform, t, sizeof(struct nk_affine_transform));
		ctx->transform_active=1;
	}
}

NK_API void 
nk_affine_translate(struct nk_context *ctx, float x, float y)
{
	NK_ASSERT(ctx);

	/*
	 * New transform is:
	 *   0 0 x
	 *   0 0 y
	 *   0 0 1
	 */
	struct nk_affine_transform t =
	{
		1,0,x,
		0,1,y
	};
	nk_set_transform(ctx, &t);
}

NK_API void 
nk_affine_scale(struct nk_context *ctx, float x, float y)
{
	NK_ASSERT(ctx);

	/*
	 * New transform is:
	 *   x 0 0
	 *   0 y 0
	 *   0 0 1
	 */
	struct nk_affine_transform t =
	{
		x,0,0,
		0,y,0
	};
	nk_set_transform(ctx, &t);
}

NK_API void 
nk_affine_shear(struct nk_context *ctx, float x, float y)
{
	NK_ASSERT(ctx); 

	/*
	 * New transform is:
	 *   1 x 0
	 *   y 1 0
	 *   0 0 1
	 */
	struct nk_affine_transform t =
	{
		1,x,0,
		y,1,0
	};
	nk_set_transform(ctx, &t);
}

NK_API void 
nk_affine_rotate(struct nk_context *ctx, float n)
{
	NK_ASSERT(ctx); 

	/*
	 * New transform is:
	 *   cos(d) -sin(d) 0
	 *   sin(d) cos(d) 0
	 *   0 0 1
	 */
	float cos_n = NK_COS(n);
	float sin_n = NK_SIN(n);

	struct nk_affine_transform t =
	{
		cos_n, -sin_n, 0,
		sin_n, cos_n, 0
	};
	nk_set_transform(ctx, &t);
}

NK_API void 
nk_affine_clear(struct nk_context *ctx)
{
	NK_ASSERT(ctx); 
	NK_MEMSET(&ctx->transform,0,sizeof(struct nk_affine_transform));
	ctx->transform_active=0;
}

NK_API struct 
nk_affine_transform nk_affine_get(struct nk_context *ctx)
{
	NK_ASSERT(ctx); 
	return ctx->transform;
}

NK_API void 
nk_affine_set(struct nk_context *ctx, const struct nk_affine_transform *t)
{
	NK_ASSERT(ctx); 
	NK_MEMCPY(&ctx->transform, t, sizeof(struct nk_affine_transform));
	ctx->transform_active=0;
}
#endif

