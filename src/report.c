#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "report.h"

#define REPORT_LINE_MAX 4096
#define REPORT_CELL_MAX 512

static int dir_exists(const char *path) {
    struct stat st;
    return path && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int file_exists(const char *path) {
    struct stat st;
    return path && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int path_join(char *buf, size_t size, const char *dir, const char *name) {
    int n;

    if (!buf || !dir || !name || size == 0)
        return -1;

    n = snprintf(buf, size, "%s/%s", dir, name);
    return (n < 0 || (size_t)n >= size) ? -1 : 0;
}

static void html_text(FILE *fp, const char *s) {
    const unsigned char *p = (const unsigned char *)s;

    if (!s)
        return;

    while (*p) {
        switch (*p) {
            case '&': fputs("&amp;", fp); break;
            case '<': fputs("&lt;", fp); break;
            case '>': fputs("&gt;", fp); break;
            case '"': fputs("&quot;", fp); break;
            case '\'': fputs("&#39;", fp); break;
            default: fputc(*p, fp); break;
        }
        p++;
    }
}

static int csv_next_cell(const char **cursor, char *cell, size_t cell_size) {
    const char *p;
    size_t n = 0;
    int quoted = 0;
    int closed = 0;

    if (!cursor || !cell || cell_size == 0)
        return -1;
    p = *cursor;
    if (!p)
        return -1;
    if (*p == '\0' || *p == '\n' || *p == '\r')
        return 0;

    if (*p == '"') {
        quoted = 1;
        p++;
    }

    while (*p) {
        if (quoted) {
            if (*p == '"' && p[1] == '"') {
                if (n + 1 >= cell_size)
                    return -1;
                cell[n++] = '"';
                p += 2;
                continue;
            }
            if (*p == '"') {
                closed = 1;
                p++;
                if (*p == ',')
                    p++;
                else if (*p != '\0' && *p != '\n' && *p != '\r')
                    return -1;
                break;
            }
            if (*p == '\n' || *p == '\r')
                return -1;
        } else if (*p == ',' || *p == '\n' || *p == '\r') {
            if (*p == ',')
                p++;
            break;
        } else if (*p == '"') {
            return -1;
        }

        if (n + 1 >= cell_size)
            return -1;
        cell[n++] = *p;
        p++;
    }

    if (quoted && !closed)
        return -1;

    cell[n] = '\0';
    *cursor = p;
    return 1;
}

static void write_csv_table(FILE *html, const char *csv_path, const char **columns,
                            int column_count, const char *empty_msg) {
    FILE *csv;
    char line[REPORT_LINE_MAX];
    char headers[16][REPORT_CELL_MAX];
    int indexes[16];
    int header_count = 0;
    int i;
    int rows = 0;

    csv = fopen(csv_path, "r");
    if (!csv) {
        fputs("<p class=\"muted\">", html);
        html_text(html, empty_msg);
        fputs("</p>\n", html);
        return;
    }

    if (!fgets(line, sizeof(line), csv)) {
        fclose(csv);
        fputs("<p class=\"muted\">CSV file is empty.</p>\n", html);
        return;
    }

    {
        const char *cur = line;
        int status = 0;
        while (header_count < 16 &&
               (status = csv_next_cell(&cur, headers[header_count],
                                       sizeof(headers[header_count]))) > 0)
            header_count++;
        if (status < 0) {
            fclose(csv);
            fputs("<p class=\"muted\">Invalid CSV header.</p>\n", html);
            return;
        }
    }

    for (i = 0; i < column_count; i++) {
        int j;
        indexes[i] = -1;
        for (j = 0; j < header_count; j++) {
            if (strcmp(columns[i], headers[j]) == 0) {
                indexes[i] = j;
                break;
            }
        }
    }

    fputs("<table><thead><tr>", html);
    for (i = 0; i < column_count; i++) {
        fputs("<th>", html);
        html_text(html, columns[i]);
        fputs("</th>", html);
    }
    fputs("</tr></thead><tbody>\n", html);

    while (fgets(line, sizeof(line), csv)) {
        char cells[16][REPORT_CELL_MAX];
        int cell_count = 0;
        int status = 0;
        const char *cur = line;

        while (cell_count < 16 &&
               (status = csv_next_cell(&cur, cells[cell_count],
                                       sizeof(cells[cell_count]))) > 0)
            cell_count++;

        if (status < 0 || cell_count == 0)
            continue;

        fputs("<tr>", html);
        for (i = 0; i < column_count; i++) {
            fputs("<td>", html);
            if (indexes[i] >= 0 && indexes[i] < cell_count)
                html_text(html, cells[indexes[i]]);
            fputs("</td>", html);
        }
        fputs("</tr>\n", html);
        rows++;
    }

    fputs("</tbody></table>\n", html);
    if (rows == 0)
        fputs("<p class=\"muted\">No rows found.</p>\n", html);

    fclose(csv);
}

static void write_search_table(FILE *html, const char *csv_path) {
    FILE *csv;
    char line[REPORT_LINE_MAX];
    int rows = 0;

    csv = fopen(csv_path, "r");
    if (!csv) {
        fputs("<p class=\"muted\">No Top-K search CSV found. Run scripts/demo.sh first.</p>\n", html);
        return;
    }

    fgets(line, sizeof(line), csv);

    fputs("<table><thead><tr><th>rank</th><th>image path</th><th>distance</th></tr></thead><tbody>\n", html);
    while (fgets(line, sizeof(line), csv)) {
        char cells[16][REPORT_CELL_MAX];
        int cell_count = 0;
        int status = 0;
        const char *cur = line;

        while (cell_count < 16 &&
               (status = csv_next_cell(&cur, cells[cell_count],
                                       sizeof(cells[cell_count]))) > 0)
            cell_count++;

        if (status < 0 || cell_count < 6)
            continue;

        fputs("<tr><td>", html);
        html_text(html, cells[0]);
        fputs("</td><td>", html);
        html_text(html, cells[5]);
        fputs("</td><td>", html);
        html_text(html, cells[4]);
        fputs("</td></tr>\n", html);
        rows++;
    }
    fputs("</tbody></table>\n", html);

    if (rows == 0)
        fputs("<p class=\"muted\">No Top-K rows found.</p>\n", html);

    fclose(csv);
}

static void write_image_card(FILE *fp, const char *output_dir,
                             const char *filename, const char *title) {
    char path[1024];

    if (path_join(path, sizeof(path), output_dir, filename) != 0 || !file_exists(path))
        return;

    fputs("<figure class=\"card\"><img src=\"", fp);
    html_text(fp, filename);
    fputs("\" alt=\"", fp);
    html_text(fp, title);
    fputs("\"><figcaption>", fp);
    html_text(fp, title);
    fputs("<span>", fp);
    html_text(fp, filename);
    fputs("</span></figcaption></figure>\n", fp);
}

static void write_sample_list(FILE *html, const char *metadata_csv) {
    FILE *csv;
    char line[REPORT_LINE_MAX];
    int count = 0;

    csv = fopen(metadata_csv, "r");
    if (!csv) {
        fputs("<p class=\"muted\">No metadata CSV found.</p>\n", html);
        return;
    }

    fgets(line, sizeof(line), csv);
    fputs("<ul class=\"samples\">\n", html);
    while (fgets(line, sizeof(line), csv)) {
        char cell[REPORT_CELL_MAX];
        char path[REPORT_CELL_MAX] = "";
        const char *cur = line;
        int col = 0;
        int status = 0;

        while ((status = csv_next_cell(&cur, cell, sizeof(cell))) > 0) {
            if (col == 2) {
                snprintf(path, sizeof(path), "%s", cell);
                break;
            }
            col++;
        }

        if (status < 0)
            continue;

        if (path[0]) {
            fputs("<li>", html);
            html_text(html, path);
            fputs("</li>\n", html);
            count++;
        }
    }
    fputs("</ul>\n", html);

    if (count == 0)
        fputs("<p class=\"muted\">No sample images found.</p>\n", html);

    fclose(csv);
}

static void write_original_cards(FILE *html, const char *metadata_csv) {
    FILE *csv;
    char line[REPORT_LINE_MAX];
    int count = 0;

    csv = fopen(metadata_csv, "r");
    if (!csv) {
        fputs("<p class=\"muted\">No original image metadata found.</p>\n", html);
        return;
    }

    fgets(line, sizeof(line), csv);
    fputs("<div class=\"grid\">\n", html);
    while (fgets(line, sizeof(line), csv)) {
        char cell[REPORT_CELL_MAX];
        char path[REPORT_CELL_MAX] = "";
        const char *cur = line;
        int col = 0;
        int status = 0;

        while ((status = csv_next_cell(&cur, cell, sizeof(cell))) > 0) {
            if (col == 2) {
                snprintf(path, sizeof(path), "%s", cell);
                break;
            }
            col++;
        }

        if (status < 0)
            continue;

        if (path[0]) {
            fputs("<figure class=\"card\"><img src=\"../", html);
            html_text(html, path);
            fputs("\" alt=\"Original image\"><figcaption>Original<span>", html);
            html_text(html, path);
            fputs("</span></figcaption></figure>\n", html);
            count++;
        }
    }
    fputs("</div>\n", html);

    if (count == 0)
        fputs("<p class=\"muted\">No original images found.</p>\n", html);

    fclose(csv);
}

report_status_t generate_html_report_status(const char *output_dir,
                                            const char *report_path) {
    FILE *fp;
    char metadata_csv[1024];
    char search_csv[1024];
    char time_buf[64];
    time_t now;
    struct tm *tm_info;
    const char *metadata_cols[] = {"id", "path", "width", "height", "format"};

    if (!dir_exists(output_dir))
        return REPORT_STATUS_OUTPUT_DIR_MISSING;
    if (!report_path)
        return REPORT_STATUS_PATH_REQUIRED;

    if (path_join(metadata_csv, sizeof(metadata_csv), output_dir,
                  "demo_metadata.csv") != 0 ||
        path_join(search_csv, sizeof(search_csv), output_dir,
                  "demo_search.csv") != 0)
        return REPORT_STATUS_PATH_TOO_LONG;

    fp = fopen(report_path, "w");
    if (!fp)
        return REPORT_STATUS_OPEN_FAILED;

    now = time(NULL);
    tm_info = localtime(&now);
    if (tm_info)
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
    else
        snprintf(time_buf, sizeof(time_buf), "unknown");

    fputs("<!doctype html>\n<html lang=\"en\">\n<head>\n", fp);
    fputs("<meta charset=\"utf-8\">\n", fp);
    fputs("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n", fp);
    fputs("<title>C-ImageDB Demo Report</title>\n", fp);
    fputs("<style>\n", fp);
    fputs("body{font-family:-apple-system,BlinkMacSystemFont,\"Segoe UI\",sans-serif;margin:0;background:#f6f7f9;color:#17202a}main{max-width:1120px;margin:0 auto;padding:32px 20px}h1{font-size:32px;margin:0 0 8px}h2{margin-top:32px;border-bottom:1px solid #d8dde5;padding-bottom:8px}.muted{color:#687386}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:16px}.card{margin:0;background:white;border:1px solid #d8dde5;border-radius:8px;overflow:hidden}.card img{display:block;width:100%;height:180px;object-fit:contain;background:#111}.card figcaption{display:flex;justify-content:space-between;gap:12px;padding:10px 12px;font-weight:600}.card span{font-weight:400;color:#687386}table{width:100%;border-collapse:collapse;background:white;border:1px solid #d8dde5}th,td{text-align:left;border-bottom:1px solid #e7ebf0;padding:9px 10px;font-size:14px}th{background:#eef2f6}.samples{background:white;border:1px solid #d8dde5;border-radius:8px;padding:14px 28px}.note{background:#fff8df;border:1px solid #ead58a;border-radius:8px;padding:12px 14px}.summary{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:12px}.summary div{background:white;border:1px solid #d8dde5;border-radius:8px;padding:12px}\n", fp);
    fputs("</style>\n</head>\n<body><main>\n", fp);

    fputs("<h1>C-ImageDB Demo Report</h1>\n<p class=\"muted\">Generated at ", fp);
    html_text(fp, time_buf);
    fputs("</p>\n", fp);
    fputs("<p class=\"note\">Images are linked as PPM/BMP artifacts. Some browsers may download PPM files instead of previewing them inline.</p>\n", fp);

    fputs("<section><h2>Input Samples</h2>\n", fp);
    write_sample_list(fp, metadata_csv);
    fputs("</section>\n", fp);

    fputs("<section><h2>Original Images</h2>\n", fp);
    write_original_cards(fp, metadata_csv);
    fputs("</section>\n", fp);

    fputs("<section><h2>Image Results</h2><div class=\"grid\">\n", fp);
    write_image_card(fp, output_dir, "demo_gray.ppm", "Grayscale");
    write_image_card(fp, output_dir, "demo_edge.ppm", "Sobel Edge Detection");
    write_image_card(fp, output_dir, "demo_hist.ppm", "RGB Histogram");
    write_image_card(fp, output_dir, "demo_contact.ppm", "Top-K Contact Sheet");
    fputs("</div></section>\n", fp);

    fputs("<section><h2>Top-K Similar Search</h2>\n", fp);
    fputs("<p class=\"muted\">Source: demo_search.csv</p>\n", fp);
    write_search_table(fp, search_csv);
    fputs("</section>\n", fp);

    fputs("<section><h2>Metadata Summary</h2>\n", fp);
    fputs("<p class=\"muted\">Source: demo_metadata.csv</p>\n", fp);
    write_csv_table(fp, metadata_csv, metadata_cols, 5,
                    "No metadata.csv content found. Run metadata export first.");
    fputs("</section>\n", fp);

    fputs("<section><h2>Test Output Summary</h2><div class=\"summary\">\n", fp);
    fputs("<div><strong>Build</strong><br>make clean && make</div>\n", fp);
    fputs("<div><strong>Demo artifacts</strong><br>gray, edge, histogram, contact sheet</div>\n", fp);
    fputs("<div><strong>CSV exports</strong><br>metadata.csv and Top-K search results</div>\n", fp);
    fputs("<div><strong>Report</strong><br>output/index.html generated successfully</div>\n", fp);
    fputs("</div></section>\n", fp);

    fputs("</main></body>\n</html>\n", fp);

    if (fclose(fp) != 0)
        return REPORT_STATUS_FINISH_FAILED;

    return REPORT_STATUS_OK;
}

int generate_html_report(const char *output_dir, const char *report_path) {
    report_status_t status = generate_html_report_status(output_dir, report_path);

    switch (status) {
        case REPORT_STATUS_OK:
            return 0;
        case REPORT_STATUS_OUTPUT_DIR_MISSING:
            fprintf(stderr, "[ERROR] Output directory does not exist: %s\n",
                    output_dir ? output_dir : "(null)");
            break;
        case REPORT_STATUS_PATH_REQUIRED:
            fprintf(stderr, "[ERROR] Report path is required\n");
            break;
        case REPORT_STATUS_PATH_TOO_LONG:
            fprintf(stderr, "[ERROR] Report path is too long\n");
            break;
        case REPORT_STATUS_OPEN_FAILED:
            fprintf(stderr, "[ERROR] Cannot write report: %s\n", report_path);
            break;
        case REPORT_STATUS_FINISH_FAILED:
            fprintf(stderr, "[ERROR] Failed to finish writing report: %s\n",
                    report_path);
            break;
    }
    return -1;
}
