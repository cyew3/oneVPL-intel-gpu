//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014-2016 Intel Corporation. All Rights Reserved.
//
/********************************************************************************
* 
* File: FaceDet.cpp
*
* Routines for Viola-Jones face detection
* 
********************************************************************************/
#include "FaceDet.h"
#include <math.h>
#include <immintrin.h>
#include <float.h>

int init_haar_classifier(haarclass_cascade **ret, const haar_cascade *cascade) 
{
    haarclass_classifier *ptr;
    int sz, num_features = 0;
    int i, j;

    for (i=0; i<cascade->num_stages; i++) {
        num_features+= cascade->stage[i].num_classifiers;
    }

    sz =sizeof(haarclass_cascade) +
        sizeof(haarclass_stage)*cascade->num_stages +
        sizeof(haarclass_classifier)*num_features;

    *ret = (haarclass_cascade*) malloc(sz);
    if (*ret == NULL) return 1;     // Error
    memset(*ret, 0, sz);

    haarclass_cascade *res = *ret;

    //initialize header
    res->num_stages = cascade->num_stages;
    res->stage = (haarclass_stage*)(res + 1);
    ptr = (haarclass_classifier*)(res->stage + cascade->num_stages);

    //initialize classifier
    for (i=0; i<cascade->num_stages; i++) {
        const haar_stage* stage = cascade->stage + i;
        haarclass_stage* class_stage = res->stage + i;

        class_stage->num_classifiers = stage->num_classifiers;
        class_stage->stage_threshold = stage->stage_threshold - 0.0001f;
        class_stage->simple			 = 1;
        class_stage->classifier		 = ptr;
        ptr+= stage->num_classifiers;

        for (j=0; j<stage->num_classifiers; j++) {
            const haar_classifier* classifier = stage->classifier + j;
            haarclass_classifier* class_classifier = class_stage->classifier + j;

            memset(class_classifier, -1, sizeof(*class_classifier));

            class_classifier->threshold	= classifier->threshold;
            class_classifier->alpha[0]	= classifier->alpha[0];
            class_classifier->alpha[1]	= classifier->alpha[1];

            if(classifier->feature.num_rects==2) {
                memset( &(class_classifier->feature.rect[2]), 0, sizeof(class_classifier->feature.rect[2]) );
            }
            else {
                class_stage->simple = 0;
            }
        }
    }
    return 0;                       // Success
}

//normalize contrast
void normalize_contrast(BYTE *img_in, BYTE *img_out, int w, int h) 
{
    int map[256], histo[256] = {0};
    int i, sz, sum;
    double scl;

    sz = w*h;
    for (i=0; i<sz; i++) histo[img_in[i]]++;

    i = 0;
    while (!histo[i]) ++i;

    if (histo[i]==sz) {
        memset(img_out, i, sizeof(BYTE)*(sz));
        return;
    }

    //derive scaling factor
    scl = 255.0/(sz - histo[i]);

    //compute color map
    sum = 0;
    for (map[i++]=0; i<256; ++i) {
        sum += histo[i];
        map[i] = round_double(sum * scl);
    }

    //map colors to output image
    for (i=0; i<sz; ++i) img_out[i] = (BYTE)map[img_in[i]];
}

//compute integral image
void integral_image(BYTE *src, size_t src_step, int *sum, size_t sum_step, double *sqsum, size_t sqsum_step, int w, int h) 
{
    int srcstep		= (int)(src_step/sizeof(BYTE));
    int sumstep		= (int)(sum_step/sizeof(int));
    int sqsumstep	= (int)(sqsum_step/sizeof(double)); 
    double sq, tq;
    int s, t, x, y, k;
    BYTE it;

    //initialize first row
    memset(sum,   0, (w + 1)*sizeof(int   )); sum   += sumstep   + 1;
    memset(sqsum, 0, (w + 1)*sizeof(double)); sqsum += sqsumstep + 1;

    //create sum and squared sum images
    for (y=0; y<h; y++, src+= srcstep-1, sum+= sumstep-1, sqsum+= sqsumstep-1) {
        for (k=0; k<1; k++, src++, sum++, sqsum++) {
            s = sum[-1] = 0;
            sq = sqsum[-1] = 0;
            for (x=0; x<w; x++) {
                it = src[x];
                s+= it;
                sq+= (double)it*it;
                t = sum[x - sumstep] + s;
                tq = sqsum[x - sqsumstep] + sq;
                sum[x] = t;
                sqsum[x] = tq;
            }
        }
    }
}

//preprocess integral image sums
void preprocess_integral_image_sums(haar_cascade *mcascade, haarclass_cascade *class_cascade, const int *sum, size_t sum_step, const double *sqsum, size_t sqsum_step, double scl) 
{
    int ex, ey, ew, eh, i, j, k, l, n;
    double wt_scl, s;
    const haar_feature *feat;
    haarclass_feature *cls_feat;
    int rx[3], ry[3], rw[3], rh[3];
    int ax, ay, sx, sy, x, y;
    int bw, bh, sbw, sbh;
    int tx, ty, tw, th;

    mcascade->scl = scl;
    mcascade->scaled_win_width  = round_double(mcascade->win_width  * scl);
    mcascade->scaled_win_height = round_double(mcascade->win_height * scl);

    class_cascade->sum = (int*)sum;
    class_cascade->sqsum = (double*)sqsum;

    ex = ey = round_double(scl);
    ew = round_double((mcascade->win_width  - 2)*scl);
    eh = round_double((mcascade->win_height - 2)*scl);
    wt_scl = 1.0/(ew*eh);
    class_cascade->inv_window_area = wt_scl;

    class_cascade->a = (int*)sum + (size_t)sum_step*(ey)	+ (ex);
    class_cascade->b = (int*)sum + (size_t)sum_step*(ey)	+ (ex+ew);
    class_cascade->c = (int*)sum + (size_t)sum_step*(ey+eh) + (ex);
    class_cascade->d = (int*)sum + (size_t)sum_step*(ey+eh) + (ex+ew);

    class_cascade->A = (double*)sqsum + (size_t)sqsum_step*(ey)	   + (ex);
    class_cascade->B = (double*)sqsum + (size_t)sqsum_step*(ey)	   + (ex+ew);
    class_cascade->C = (double*)sqsum + (size_t)sqsum_step*(ey+eh) + (ex);
    class_cascade->D = (double*)sqsum + (size_t)sqsum_step*(ey+eh) + (ex+ew);

    for (i=0; i<mcascade->num_stages; i++) {
        for (j=0; j<mcascade->stage[i].num_classifiers; j++) {
            feat = &mcascade->stage[i].classifier[j].feature;
            cls_feat = &class_cascade->stage[i].classifier[j].feature;

            bw = -1, bh = -1;
            for (k=0; k<3; k++) {
                if (!cls_feat->rect[k].a) break;
                rx[k] = feat->rect[k].x;
                ry[k] = feat->rect[k].y;
                rw[k] = feat->rect[k].width;
                rh[k] = feat->rect[k].height;
                bw = (int)FMIN((uint)bw, (uint)(rw[k] - 1        ));
                bw = (int)FMIN((uint)bw, (uint)(rx[k] - rx[0] - 1));
                bh = (int)FMIN((uint)bh, (uint)(rh[k] - 1        ));
                bh = (int)FMIN((uint)bh, (uint)(ry[k] - ry[0] - 1));
            }
            bw++; bh++;

            sx = sy = sbw = sbh = 0;
            ax = rw[0]/bw; ay = rh[0]/bh;
            if (ax <= 0) { sbw = round_double(rw[0]*scl) / ax; x = round_double(rx[0] * scl); sx = 1; }
            if (ay <= 0) { sbh = round_double(rh[0]*scl) / ay; y = round_double(ry[0] * scl); sy = 1; }

            l = k; s = 0.0;
            for (k=0; k<l; k++) {

                if (sx) { tx = (rx[k]-rx[0])*sbw / bw + x; tw = rw[k] * sbw / bw; } else { tx = round_double(rx[k]*scl); tw = round_double(rw[k]*scl); }
                if (sy) { ty = (ry[k]-ry[0])*sbh / bh + y; th = rh[k] * sbh / bh; } else { ty = round_double(ry[k]*scl); th = round_double(rh[k]*scl); }

                cls_feat->rect[k].a = (int*)sum + (size_t)sum_step*(ty   ) + (tx   );
                cls_feat->rect[k].b = (int*)sum + (size_t)sum_step*(ty   ) + (tx+tw);
                cls_feat->rect[k].c = (int*)sum + (size_t)sum_step*(ty+th) + (tx   );
                cls_feat->rect[k].d = (int*)sum + (size_t)sum_step*(ty+th) + (tx+tw);
                cls_feat->rect[k].weight = feat->rect[k].weight * wt_scl;

                if (k==0) n = tw*th;
                else s+= cls_feat->rect[k].weight * tw*th;
            }
            cls_feat->rect[0].weight = -s/n;
        }
    }
}

//compute sum of the integral image
int run_haar_classifier_cascade_sum(const haar_cascade* _cascade, const haarclass_cascade* cascade, int pt_x, int pt_y, double &stage_sum, int start_stage, size_t sum_step, int sum_width, int sum_height, size_t sqsum_step) 
{
    int p_offset, pq_offset;
    int i, j;
    double mean, variance_norm_factor;

    if (pt_x < 0 || pt_y < 0 ||
        pt_x + _cascade->scaled_win_width  >= sum_width ||
        pt_y + _cascade->scaled_win_height >= sum_height )
        return -1;

    p_offset  = pt_y * (int)(sum_step/sizeof(int)) + pt_x;
    pq_offset = pt_y * (int)(sqsum_step/sizeof(double)) + pt_x;

    mean = (cascade->a[p_offset] - 
        cascade->b[p_offset] - 
        cascade->c[p_offset] + 
        cascade->d[p_offset])*cascade->inv_window_area;

    variance_norm_factor =	cascade->A[pq_offset] - 
        cascade->B[pq_offset] -
        cascade->C[pq_offset] + 
        cascade->D[pq_offset];

    variance_norm_factor = variance_norm_factor*cascade->inv_window_area - mean*mean;

    if (variance_norm_factor >= 0.) variance_norm_factor = sqrt(variance_norm_factor);
    else variance_norm_factor = 1.;

    if (g_FS_OPT_SSE4) { //SSE implementation
        for (i=start_stage; i<cascade->num_stages; i++) {
            __m128d vstage_sum = _mm_setzero_pd();
            if (cascade->stage[i].simple) {
                for (j=0; j<cascade->stage[i].num_classifiers; j++) {
                    haarclass_classifier *classifier = cascade->stage[i].classifier + j;

                    double sum1, sum2;
                    sum1= (classifier->feature.rect[0].a[p_offset] - 
                        classifier->feature.rect[0].b[p_offset] - 
                        classifier->feature.rect[0].c[p_offset] + 
                        classifier->feature.rect[0].d[p_offset])*classifier->feature.rect[0].weight;
                    sum2= (classifier->feature.rect[1].a[p_offset] - 
                        classifier->feature.rect[1].b[p_offset] - 
                        classifier->feature.rect[1].c[p_offset] + 
                        classifier->feature.rect[1].d[p_offset])*classifier->feature.rect[1].weight;

                    __m128d t = _mm_set_sd(classifier->threshold*variance_norm_factor);
                    __m128d a = _mm_set_sd(classifier->alpha[0]);
                    __m128d b = _mm_set_sd(classifier->alpha[1]);
                    __m128d sum = _mm_set_sd(sum1 + sum2);
                    t = _mm_cmpgt_sd(t, sum);
                    vstage_sum = _mm_add_sd(vstage_sum, _mm_blendv_pd(b, a, t));
                }
            }
            else {
                for (j=0; j<cascade->stage[i].num_classifiers; j++) {
                    haarclass_classifier *classifier = cascade->stage[i].classifier + j;

                    double sum1, sum2, sum3 = 0;
                    sum1= (classifier->feature.rect[0].a[p_offset] - 
                        classifier->feature.rect[0].b[p_offset] - 
                        classifier->feature.rect[0].c[p_offset] + 
                        classifier->feature.rect[0].d[p_offset])*classifier->feature.rect[0].weight;
                    sum2= (classifier->feature.rect[1].a[p_offset] - 
                        classifier->feature.rect[1].b[p_offset] - 
                        classifier->feature.rect[1].c[p_offset] + 
                        classifier->feature.rect[1].d[p_offset])*classifier->feature.rect[1].weight;
                    if (classifier->feature.rect[2].a) {
                        sum3= (classifier->feature.rect[2].a[p_offset] - 
                            classifier->feature.rect[2].b[p_offset] -
                            classifier->feature.rect[2].c[p_offset] + 
                            classifier->feature.rect[2].d[p_offset])*classifier->feature.rect[2].weight;
                    }

                    __m128d t = _mm_set_sd(classifier->threshold*variance_norm_factor);
                    __m128d a = _mm_set_sd(classifier->alpha[0]);
                    __m128d b = _mm_set_sd(classifier->alpha[1]);

                    __m128d sum = _mm_set_sd(sum1 + sum2 + sum3);
                    t = _mm_cmpgt_sd(t, sum);
                    vstage_sum = _mm_add_sd(vstage_sum, _mm_blendv_pd(b, a, t));
                }
            }

            __m128d i_threshold = _mm_set1_pd(cascade->stage[i].stage_threshold);
            if (_mm_comilt_sd(vstage_sum, i_threshold)) return -i;
        }
    }
    else { //C implementation
        for (i=start_stage; i<cascade->num_stages; i++) {
            stage_sum = 0.0;
            if (cascade->stage[i].simple) {
                for (j=0; j<cascade->stage[i].num_classifiers; j++) {
                    haarclass_classifier *classifier = cascade->stage[i].classifier + j;
                    double t = classifier->threshold * variance_norm_factor;
                    double sum;

                    sum = (classifier->feature.rect[0].a[p_offset] - 
                        classifier->feature.rect[0].b[p_offset] - 
                        classifier->feature.rect[0].c[p_offset] + 
                        classifier->feature.rect[0].d[p_offset])*classifier->feature.rect[0].weight;

                    sum+= (classifier->feature.rect[1].a[p_offset] - 
                        classifier->feature.rect[1].b[p_offset] - 
                        classifier->feature.rect[1].c[p_offset] + 
                        classifier->feature.rect[1].d[p_offset])*classifier->feature.rect[1].weight;

                    stage_sum+= classifier->alpha[sum >= t];
                }
            }
            else {
                for(j=0; j<cascade->stage[i].num_classifiers; j++) {
                    haarclass_classifier* classifier = cascade->stage[i].classifier + j;
                    double t = classifier->threshold*variance_norm_factor;
                    double sum;

                    sum = (classifier->feature.rect[0].a[p_offset] - 
                        classifier->feature.rect[0].b[p_offset] -
                        classifier->feature.rect[0].c[p_offset] +
                        classifier->feature.rect[0].d[p_offset])*classifier->feature.rect[0].weight;

                    sum+= (classifier->feature.rect[1].a[p_offset] - 
                        classifier->feature.rect[1].b[p_offset] -
                        classifier->feature.rect[1].c[p_offset] +
                        classifier->feature.rect[1].d[p_offset])*classifier->feature.rect[1].weight;

                    if (classifier->feature.rect[2].a) {
                        sum+= (classifier->feature.rect[2].a[p_offset] - 
                            classifier->feature.rect[2].b[p_offset] -
                            classifier->feature.rect[2].c[p_offset] + 
                            classifier->feature.rect[2].d[p_offset])*classifier->feature.rect[2].weight;
                    }

                    stage_sum+= classifier->alpha[sum >= t];
                }
            }

            if (stage_sum < cascade->stage[i].stage_threshold) return -i;
        }
    }
    return 1;
}

//detect faces using scaled cascade
void detect_faces_scale_cascade(const haar_cascade *cascade, 
                                const haarclass_cascade* class_cascade,
                                int win_w, int win_h,
                                int x_end, 
                                int y_end,
                                double step,
                                int width,
                                int height,
                                std::vector<Rct>& vec) 
{
    int x, y, i, j, n, res;
    double stage_sum = 0;
    Rct r; 

    for (i=0; i<y_end; i++) {

        n = 1;
        y = round_double(i*step);

        for (j=0; j<x_end; j+=n) {

            x = round_double(j*step);
            res = run_haar_classifier_cascade_sum(cascade, class_cascade, x, y, stage_sum, 0, sizeof(int)*width, width, height, sizeof(double)*width);

            if (res>0) {
                r.x = x; r.y = y;
                r.w  = win_w; r.h = win_h;
                vec.push_back(r);
            }
            if (res!=0)	n = 1;
            else		n = 2;
        }
    }
}

//determines if two rectangles are similar
int similar(const Rct& a, const Rct& b) 
{
    double T; int res;
    T = (T_PR * 0.5) * ( MIN(a.w, b.w) + MIN(a.h, b.h) );
    res = ABS(a.x - b.x)<=T && ABS(a.y - b.y)<=T && ABS((a.x + a.w) - (b.x + b.w))<=T && ABS((a.y + a.h) - (b.y + b.h))<=T;
    return res;
}

//splits the sequence into equivalence classes
int split_seq(const std::vector<Rct>& seq, std::vector<int>& inds) 
{
    int i, j, k, n;
    int x1, x2, y1, y2;
    const Rct* list;
    int prev, *n1, *n2;
    int res = 0;

    //allocate working arrays
    n  = (int)seq.size();
    n1 = (int*)malloc(sizeof(int) * n);
    n2 = (int*)malloc(sizeof(int) * n);

    //initialize working arrays
    list = &(seq[0]);
    for (i=0; i<n; i++) n1[i] = -1;
    memset(n2, 0, sizeof(int) * n);

    //splitting
    for (i=0; i<n; i++) {
        x1 = i;
        while (n1[x1]>=0) x1 = n1[x1];

        for (j=0; j<n; j++) {
            if (i==j || !similar(list[i], list[j])) continue;

            x2 = j;
            while (n1[x2]>=0) x2 = n1[x2];

            if (x2 != x1) {
                y1 = n2[x1];
                y2 = n2[x2];
                if (y1 > y2) n1[x2] = x1;
                else { n1[x1] = x2; n2[x2]+= y1 == y2; x1 = x2; }
                k = j; while (prev = n1[k], n1[k]>=0) { n1[k] = x1; k = prev; }
                k = i; while (prev = n1[k], n1[k]>=0) { n1[k] = x1; k = prev; }
            }
        }
    }
    inds.resize(n);
    for (i=0; i<n; i++) {
        x1 = i;
        while (n1[x1]>=0) x1 = n1[x1];
        if (n2[x1]>=0) n2[x1] = ~res++;
        inds[i] = ~n2[x1];
    }

    //free working arrays
    free(n1); 
    free(n2);

    //return number of equivalence classes
    return res;
}

//do pruning of detected face areas
void prune_face_areas(std::vector<Rct>& faces, int T, std::vector<int>* wts) 
{
    std::vector<int> inds;
    size_t k;
    int i, j, ni, n;
    Rct r, r1, r2;
    int rwt1, rwt2;
    double wt, wt1;
    int sw, sh;

    if (T<=0 || faces.empty()) {
        if (wts) {
            wts->resize(faces.size());
            for (k=0; k<faces.size(); k++) (*wts)[k] = 1;
        }
        return;
    }

    n = split_seq(faces, inds);
    std::vector<Rct   >	rcts(n);
    std::vector<int   >	rwts(n, 0);
    std::vector<double>	ewts(n, DBL_MIN);

    ni = (int)inds.size();

    for (i=0; i<ni; i++) {
        j = inds[i];
        rcts[j].x+= faces[i].x;
        rcts[j].y+= faces[i].y;
        rcts[j].w+= faces[i].w;
        rcts[j].h+= faces[i].h;
        rwts[j]++;
    }

    for (i=0; i<n; i++) {
        r = rcts[i];
        wt = 1.f/rwts[i];
        rcts[i].x = round_double(r.x*wt);
        rcts[i].y = round_double(r.y*wt);
        rcts[i].w = round_double(r.w*wt);
        rcts[i].h = round_double(r.h*wt);
    }

    faces.clear();
    if (wts) wts->clear();

    for (i=0; i<n; i++) {
        r1	 = rcts[i];
        rwt1 = rwts[i];
        wt1	 = ewts[i];
        if (rwt1<=T) continue;

        for (j=0; j<n; j++) {
            rwt2 = rwts[j];
            if (j==i || rwt2<=T) continue;

            r2 = rcts[j];
            sw = round_double(r2.w * T_PR);
            sh = round_double(r2.h * T_PR);

            if (i!=j && 
                r1.x>=r2.x - sw && 
                r1.y>=r2.y - sh &&
                r1.x + r1.w <= r2.x + r2.w + sw &&
                r1.y + r1.h <= r2.y + r2.h + sh &&
                (rwt2 > MAX(3, rwt1) || rwt1 < 3)) {
                    break;
            }
        }

        if (j==n) {
            faces.push_back(r1);
            if (wts) wts->push_back(rwt1);
        }
    }
}

void do_face_detection(std::vector<Rct> &faces, BYTE *frm, int w, int h, haar_cascade *cas, haarclass_cascade *cls_cas, int *sum, double *sqsum, int mwin_w, int mwin_h, int min_nboors) 
{
    double scale, scl = 1.1;
    int n = 0, win_w, win_h;
    std::vector<Rct> rects;
    std::vector<Rct> face_set;
    std::vector<int > weights;
    double step;

    //compute integral image
    integral_image(frm, sizeof(BYTE)*w, sum, sizeof(int)*(w+1), sqsum, sizeof(double)*(w+1), w, h);

    //determine num of scales
    for (n = 0, scale = 1.0; scale*cas->win_width  < w - 10 && scale*cas->win_height < h - 10; n++, scale*= scl);

    //search for faces
    scale = 1.0;
    for ( ; n-->0; scale*= scl) {
        step = MAX(2.0, scale);
        win_w = round_double(cas->win_width  * scale);
        win_h = round_double(cas->win_height * scale);
        if (win_w < mwin_w || win_h < mwin_h) continue;
        if (win_w > w || win_h > h) break;

        preprocess_integral_image_sums(cas, cls_cas, sum, w+1, sqsum, w+1, scale);

        detect_faces_scale_cascade(cas,
            cls_cas,
            win_w, win_h,
            round_double((w - win_w) / step),
            round_double((h - win_h) / step),
            step,
            w+1, h+1,
            rects);
    }

    face_set.resize(rects.size());
    if (!rects.empty()) std::copy(rects.begin(), rects.end(), face_set.begin());

    prune_face_areas(face_set, MAX(min_nboors, 1), &weights);

    faces.resize(face_set.size());
    std::copy(face_set.begin(), face_set.end(), faces.begin());
}

