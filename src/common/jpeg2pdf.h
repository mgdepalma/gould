/*
* jpeg2pdf.h - adopted from https://jpeg2pdf.sf.net
* 2024-07-03 Generations Linux <bugs@softcraft.org>
*/
#ifndef _JPEG2PDF_H_
#define _JPEG2PDF_H_

#include <gdk/gdk.h>

#define Success 0
#define Error   1

#define MAX_PDF_PAGES	 15256
#define PDF_DOT_PER_INCH 72

/* Format before each image, usually less than 142 Bytes */
#define MAX_PDF_PREFORMAT_SIZE 	256

/* Format after each image, usually less than 400 Bytes */
#define MAX_PDF_PSTFORMAT_SIZE 	512

#define XREF_ENTRY_LEN	20  /* Each XREF entry is 20 Bytes */
#define OBJNUM_EXTRA	 3  /* First Free Object; Kids Object; Catalog Object */
#define OBJNUM_PER_IMAGE 5

/* Kids Str Looks Like: "X0R", X =OBJNUM_EXTRA+OBJNUM_PER_IMAGE * (pageNum-1) */
#define MAX_KIDS_STRLEN	 10

#define MAX_PDF_XREF	(MAX_PDF_PAGES * OBJNUM_PER_IMAGE + OBJNUM_EXTRA)
#define MAX_PDF_HEADER	64	/* PDF Header, Usually less than 40 Bytes */
#define MAX_PDF_TAILER	( ( MAX_PDF_PAGES * (MAX_KIDS_STRLEN + (OBJNUM_PER_IMAGE * XREF_ENTRY_LEN)) ) + (OBJNUM_EXTRA * XREF_ENTRY_LEN) + 256 )

typedef struct _jpeg2pdf_node jpeg2pdf_node_t, *jpeg2pdf_node_ptr_t;
typedef struct _jpeg2pdf jpeg2pdf_t, *jpeg2pdf_ptr_t;

/* specified by user */
typedef enum {PageOrientationAuto, Portrait, Landscape} PageOrientation;
typedef enum {ScaleAuto, ScaleFit, ScaleFitWidth, ScaleFitHeight, ScaleReduce, ScaleReduceWidth, ScaleReduceHeight, ScaleNone} ScaleMethod;

/* how we should actually fit the image */
typedef enum {FitWidth, FitHeight, FitNone} Fit;

struct _jpeg2pdf_node {
  char    preFormat[MAX_PDF_PREFORMAT_SIZE];
  char    pstFormat[MAX_PDF_PSTFORMAT_SIZE];
  guint8  *pJpeg;
  guint32 PageObj;
  guint32 jpegSize;
  guint32 jpegW, jpegH;
  struct _jpeg2pdf_node *pNext;
};

struct _jpeg2pdf {
  /* Link list elements */
  jpeg2pdf_node_ptr_t pFirstNode;
  jpeg2pdf_node_ptr_t pLastNode;
  guint32 nodeCount;
  /* PDF elements */
  double margin;
  double maxImgW, maxImgH;
  char   pdfHeader[MAX_PDF_HEADER];
  char   pdfTailer[MAX_PDF_TAILER];		    /* 28K Bytes */
  char   pdfXREF[MAX_PDF_XREF][XREF_ENTRY_LEN + 1]; /* 27K Bytes */
  guint32 pageW, pageH, pdfObj, currentOffSet, imgObj;
};

/* pdfW, pdfH: Page Size in inch ( 1 inch = 25.4 mm ) */
jpeg2pdf_ptr_t jpeg2pdf_initialize(double pdfW, double pdfH, double margin);

int jpeg2pdf_construct(jpeg2pdf_ptr_t pPDF, guint32 imgW, guint32 imgH,
		       guint32 fileSize, guint8 *pJpeg, guint8 colors,
		       PageOrientation pageOrientation, ScaleMethod scale,
		       double dpiX, double dpiY, bool cropHeight,
		       bool cropWidth);

guint32 jpeg2pdf_metadata(jpeg2pdf_ptr_t pPDF, char *timestamp,
			  const char *title, const char *author,
			  const char *keywords, const char *subject,
			  const char *creator);

int jpeg2pdf_finalize(jpeg2pdf_ptr_t pPDF, guint8 *outPDF, guint32 *outPDFSize);

bool get_jpeg_size(unsigned char* data, unsigned int data_size,
		   unsigned short *width, unsigned short *height,
                   unsigned char *colors, double* dpiX, double* dpiY);

#endif /* _JPEG2PDF_H_ */
