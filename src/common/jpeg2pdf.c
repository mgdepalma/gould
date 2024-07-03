/*
* jpeg2pdf.c - adopted from https://jpeg2pdf.sf.net
* 2024-06-30 Generations Linux <bugs@softcraft.org>
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "jpeg2pdf.h"
#include "util.h"

#define INDEX_USE_PPDF (-1)

/*
* (private) jpeg2pdf_setxref
*/
static void
jpeg2pdf_setxref(jpeg2pdf_ptr_t pPDF, int index, int offset, char c)
{
  if(INDEX_USE_PPDF == index) index = pPDF->pdfObj;

  if ('f' == c) //space on tail required
    sprintf(pPDF->pdfXREF[index], "%010d 65535 f \n", offset);
  else
    sprintf(pPDF->pdfXREF[index], "%010d 00000 %c \n", offset, c);
} /* </jpeg2pdf_setxref> */

/*
* jpeg2pdf_initialize
* width and height in default (portrait) orientation
*/
jpeg2pdf_ptr_t
jpeg2pdf_initialize(double pdfW, double pdfH, double margin)
{
  jpeg2pdf_ptr_t pPDF = (jpeg2pdf_ptr_t)malloc(sizeof(jpeg2pdf_t));

  if (pPDF) {
    memset(pPDF, 0, sizeof(jpeg2pdf_t));
    pPDF->pageW = (guint32)(pdfW * PDF_DOT_PER_INCH);
    pPDF->pageH = (guint32)(pdfH * PDF_DOT_PER_INCH);
    pPDF->margin = margin;

    /* Maximum image size without margins */
    pPDF->maxImgW = (double) pPDF->pageW - (2 * margin * PDF_DOT_PER_INCH);
    pPDF->maxImgH = (double) pPDF->pageH - (2 * margin * PDF_DOT_PER_INCH);

    pPDF->currentOffSet = 0;
    jpeg2pdf_setxref(pPDF, 0, pPDF->currentOffSet, 'f');

    pPDF->currentOffSet += sprintf(pPDF->pdfHeader,
					"%%PDF-1.4\n%%%c%c\n", 0xFF, 0xFF);
    pPDF->imgObj = 0;
    pPDF->pdfObj = 2;  /* 0 & 1 was reserved for xref & document Root */
  }
  return pPDF;
} /* </jpeg2pdf_initialize> */

/*
* jpeg2pdf_construct
*/
int
jpeg2pdf_construct(jpeg2pdf_ptr_t pPDF, guint32 imgW, guint32 imgH,
		   guint32 fileSize, guint8 *pJpeg, guint8 colors,
		   PageOrientation pageOrientation, ScaleMethod scale,
		   double dpiX, double dpiY, bool cropHeight, bool cropWidth)
{
  jpeg2pdf_node_ptr_t pNode;
  double imgAspect, newImgW, newImgH;
  int result = Error;

  if (pPDF != NULL) {
    if (pPDF->nodeCount >= MAX_PDF_PAGES) {
      printf("%s Reached Max Page Number (%d)\n", __func__, MAX_PDF_PAGES);
      return result;
    }
    pNode = (jpeg2pdf_node_ptr_t)malloc(sizeof(jpeg2pdf_node_t));

    if (pNode != NULL) {
      guint32 nChars, currentImageObject;
      char *pFormat, lenStr[MAX_PDF_PREFORMAT_SIZE];

      pNode->jpegW = imgW;
      pNode->jpegH = imgH;
      pNode->jpegSize = fileSize;
      pNode->pJpeg = (guint8 *)malloc(pNode->jpegSize);
      pNode->pNext = NULL;
			
      if (pNode->pJpeg != NULL) {
	bool jpegPortrait, pagePortrait;
        // accounting for page orientation, in PDF units
        // (pixels at PDF_DOT_PER_INCH dpi)
	double pageWidth, pageHeight;
        // actual values accounting for page orientation, in PDF units
	double maxImgWidth, maxImgHeight;
        // jpeg dimensions (accounting for dpiX, dpiY, PDF_DOT_PER_INCH),
        // in PDF units */
	double jpegWidth, jpegHeight;
	Fit fit;

	memcpy(pNode->pJpeg, pJpeg, pNode->jpegSize);
				
	/* Image Object */
	jpeg2pdf_setxref(pPDF, INDEX_USE_PPDF, pPDF->currentOffSet, 'n');
	currentImageObject = pPDF->pdfObj;

	pPDF->currentOffSet += sprintf(pNode->preFormat, "\n%d 0 obj\n<<\n/Type /XObject\n/Subtype /Image\n/Filter /DCTDecode\n/BitsPerComponent 8\n/ColorSpace /%s\n/Width %d\n/Height %d\n/Length %d\n>>\nstream\n", pPDF->pdfObj, ((colors) ? "DeviceRGB" : "DeviceGray"), pNode->jpegW, pNode->jpegH, pNode->jpegSize);
				
	vdebug(1, "%s....\n", pNode->preFormat);
        pPDF->currentOffSet += pNode->jpegSize;
	pFormat = pNode->pstFormat;
	nChars = sprintf(pFormat, "\nendstream\nendobj\n");
	pPDF->currentOffSet += nChars;
        pFormat += nChars;
	pPDF->pdfObj++;

	/* Determine scale of the image keeping aspect ratio */
	jpegWidth = ((double)pNode->jpegW) * PDF_DOT_PER_INCH / dpiX;
	jpegHeight = ((double)pNode->jpegH) * PDF_DOT_PER_INCH / dpiY;
	imgAspect = jpegWidth / jpegHeight;

	// Determine page orientation:
	jpegPortrait = (jpegWidth <= jpegHeight);

	if (pageOrientation == PageOrientationAuto) {
	  if (scale == ScaleNone && jpegWidth <= pPDF->maxImgW 
			         && jpegHeight <= pPDF->maxImgH) {
            // image already fits into available area, don't rotate the page
            // assuming portrait orientation the most convenient for most users
	    pagePortrait = true;
	  }
          else {
	    pagePortrait = jpegPortrait;

	    if ((pPDF->maxImgW < pPDF->maxImgH) ^ (pPDF->pageW < pPDF->pageH)) {
              // very rare case: page orientation is opposite to available area
              // orientation (it's possible with differently sized margins)
              pagePortrait =! pagePortrait;
	    }
	  }
	}
        else {
	  pagePortrait=(pageOrientation==Portrait) ? true : false;
	}
	maxImgWidth = (pagePortrait) ? pPDF->maxImgW : pPDF->maxImgH;
	maxImgHeight = (pagePortrait) ? pPDF->maxImgH : pPDF->maxImgW;

	/* Determine scaling method: */
	if (scale == ScaleFit || (scale == ScaleReduce &&
		(jpegWidth > maxImgWidth || jpegHeight > maxImgHeight))) {
          /* fit jpeg to available area */
	  if (maxImgWidth/maxImgHeight > imgAspect) {
            /* available area aspect is wider than jpeg aspect */
	    fit = FitHeight;
	  }
          else {
            /* jpeg aspect is wider than available area aspect */
	    fit = FitWidth;
	  }
	}
        else if (scale == ScaleFitWidth || (scale == ScaleReduceWidth
					    && jpegWidth > maxImgWidth)) {
          fit = FitWidth;
	}
        else if (scale == ScaleFitHeight || (scale == ScaleReduceHeight
					     && jpegHeight > maxImgHeight)) {
          fit = FitHeight;
	}
        else { // don't fit, keep original dpi
          fit = FitNone;
	}

	// Scale image:
	if (fit == FitWidth) {
	  newImgW = maxImgWidth;
	  newImgH = maxImgWidth / imgAspect;
	}
        else if (fit == FitHeight) {
	  newImgW = maxImgHeight * imgAspect;
	  newImgH = maxImgHeight;
	}
        else {  // fit == FitNone
	  newImgW = jpegWidth;
	  newImgH = jpegHeight;
	}
	vdebug(1, "%s fit => %d\n", __func__, fit);

	// Set paper size from image size (possibly fitted/reduced to specific
        /* paper size) or properly rotate the page: */
	pageWidth = cropWidth ? (newImgW+pPDF->margin) :
			 (pagePortrait ? pPDF->pageW : pPDF->pageH) ;
			   pageHeight=cropHeight ? (newImgH+pPDF->margin) :
			    (pagePortrait ? pPDF->pageH : pPDF->pageW);

         if (scale == ScaleNone) {  /* hard override (DEBUG why it's needed) */
	   pageWidth = newImgW = imgW;
	   pageHeight = newImgH = imgH;
         }

	/* Page Object */
	jpeg2pdf_setxref(pPDF, INDEX_USE_PPDF, pPDF->currentOffSet, 'n');
	pNode->PageObj = pPDF->pdfObj;
	nChars = sprintf(pFormat, "%d 0 obj\n<<\n/Type /Page\n/Parent 1 0 R\n/MediaBox [0 0 %.2f %.2f]\n/Contents %d 0 R\n/Resources %d 0 R\n>>\nendobj\n", pPDF->pdfObj, pageWidth, pageHeight, pPDF->pdfObj + 1, pPDF->pdfObj + 3);

	vdebug(1, "%s", pFormat);
	pPDF->currentOffSet += nChars;
	pFormat += nChars;
	pPDF->pdfObj++;

	/* Contents Object in Page Object */
	jpeg2pdf_setxref(pPDF, INDEX_USE_PPDF, pPDF->currentOffSet, 'n');

	/* center image */
	sprintf(lenStr, "q\n1 0 0 1 %.2f %.2f cm\n%.2f 0 0 %.2f 0 0 cm\n/I%d Do\nQ", (pageWidth-newImgW) / 2, (pageHeight-newImgH) / 2, newImgW, newImgH, pPDF->imgObj);

	nChars = sprintf(pFormat, "%d 0 obj\n<<\n/Length %d 0 R\n>>\nstream\n%s\nendstream\nendobj\n", pPDF->pdfObj, pPDF->pdfObj + 1, lenStr);

	vdebug(1, "%s\n", pFormat);
	pPDF->currentOffSet += nChars;
        pFormat += nChars;
	pPDF->pdfObj++;

	/* Length Object in Contents Object */
	jpeg2pdf_setxref(pPDF, INDEX_USE_PPDF, pPDF->currentOffSet, 'n');
	nChars = sprintf(pFormat, "%d 0 obj\n%ld\nendobj\n", pPDF->pdfObj, strlen(lenStr));
	pPDF->currentOffSet += nChars;
        pFormat += nChars;
	pPDF->pdfObj++;
				
	/* Resources Object in Page Object */
	jpeg2pdf_setxref(pPDF, INDEX_USE_PPDF, pPDF->currentOffSet, 'n');
	nChars = sprintf(pFormat, "%d 0 obj\n<<\n/ProcSet [/PDF /%s]\n/XObject <</I%d %d 0 R>>\n>>\nendobj\n", pPDF->pdfObj, ((colors) ? "ImageC" : "ImageB"), pPDF->imgObj, currentImageObject);

	pPDF->currentOffSet += nChars;	pFormat += nChars;
	pPDF->pdfObj++;
	pPDF->imgObj++;

	/* Update the Link List */
	pPDF->nodeCount++;
	if (1 == pPDF->nodeCount) {
	  pPDF->pFirstNode = pNode;
	}
        else {
	  pPDF->pLastNode->pNext = pNode;
	}
	pPDF->pLastNode = pNode;

	result = Success;
      } /* pNode->pJpeg allocated OK */
    } /* pNode is valid */
  } /* pPDF is valid */

  return result;
} /* </jpeg2pdf_construct> */

/*
* jpeg2pdf_metadata
*/
guint32
jpeg2pdf_metadata(jpeg2pdf_ptr_t pPDF, char *timestamp, const char *title,
		  const char *author, const char *keywords,
		  const char *subject, const char *creator)
{
  guint32 headerSize, tailerSize, pdfSize = 0;
  char *producer = "Generations Linux";
  char *XMPmetadata;

  if (pPDF != NULL) {
    char strKids[MAX_PDF_PAGES * MAX_KIDS_STRLEN], *pTail = pPDF->pdfTailer;
    guint32 i, nChars, xrefOffSet, metadataObj, infoObj;
    jpeg2pdf_node_ptr_t pNode;

    XMPmetadata = (char *)malloc(2048 + strlen(title) + strlen(author) +
			strlen(keywords) + strlen(subject) + strlen(creator));

    nChars = sprintf(XMPmetadata,"<?xpacket begin=\"\xef\xbb\xbf\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?>\n" \
	"<x:xmpmeta xmlns:x=\"adobe:ns:meta/\">\n" \
	"<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">\n" \
	"<rdf:Description xmlns:dc=\"http://purl.org/dc/elements/1.1/\" rdf:about=\"\">\n" \
	"<dc:title>%s</dc:title>\n" \
	"<dc:subject>%s</dc:subject>\n" \
	"<dc:creator>%s</dc:creator>\n" \
	"<dc:date>%s</dc:date>\n" \
	"</rdf:Description>\n" \
	"<rdf:Description xmlns:xmp=\"http://ns.adobe.com/xap/1.0/\" rdf:about=\"\">\n" \
	"<xmp:CreateDate>%s</xmp:CreateDate>\n" \
	"<xmp:CreatorTool>%s</xmp:CreatorTool>\n" \
	"<xmp:MetadataDate>%s</xmp:MetadataDate>\n" \
	"</rdf:Description>\n" \
	"<rdf:Description xmlns:pdf=\"http://ns.adobe.com/pdf/1.3/\" rdf:about=\"\">\n" \
	"<pdf:Keywords>%s</pdf:Keywords>\n" \
	"<pdf:PDFVersion>1.4</pdf:PDFVersion>\n" \
	"<pdf:Producer>%s</pdf:Producer>\n" \
	"</rdf:Description>\n" \
	"</rdf:RDF>\n" \
	"</x:xmpmeta>\n" \
	"<?xpacket end=\"r\"?>\n", \
	title, subject, creator, timestamp, timestamp, creator,
				timestamp, keywords, producer);

    /* Metadata Object with XMP */
    metadataObj = pPDF->pdfObj;
    jpeg2pdf_setxref(pPDF, INDEX_USE_PPDF, pPDF->currentOffSet, 'n');
    nChars = sprintf(pTail, "%d 0 obj\n<<\n/Type /Metadata\n/Subtype /XML\n/Length %d\n>>\nstream\n%sendstream\nendobj\n", pPDF->pdfObj, nChars, XMPmetadata);

    free(XMPmetadata);
    pPDF->currentOffSet += nChars;
    pTail += nChars;
    pPDF->pdfObj++;

    // convert ISO9601 to PDF Info format %Y-%m-%dT%H:%M:%S%z -> %Y%m%d%H%M%S%z'
    timestamp[4]  = timestamp[5];
    timestamp[5]  = timestamp[6];
    timestamp[6]  = timestamp[8];
    timestamp[7]  = timestamp[9];
    timestamp[8]  = timestamp[11];
    timestamp[9]  = timestamp[12];
    timestamp[10] = timestamp[14];
    timestamp[11] = timestamp[15];
    timestamp[12] = timestamp[17];
    timestamp[13] = timestamp[18];
    timestamp[14] = timestamp[19];
    timestamp[15] = timestamp[20];
    timestamp[16] = timestamp[21];
    timestamp[17] = '\'';
    timestamp[18] = timestamp[23];
    timestamp[19] = timestamp[24];
    timestamp[20] = '\'';
    timestamp[21] = '\0';

    /* Info Object */
    infoObj = pPDF->pdfObj;
    jpeg2pdf_setxref(pPDF, INDEX_USE_PPDF, pPDF->currentOffSet, 'n');

    nChars = sprintf(pTail, "%d 0 obj\n<<\n/Title (%s)\n/Author (%s)\n/Keywords (%s)\n/Subject (%s)\n/Producer (%s)\n/Creator (%s)\n/CreationDate (D:%s)\n/ModDate (D:%s)\n>>\nendobj\n", pPDF->pdfObj, title, author, keywords, subject, producer, creator, timestamp, timestamp);

    pPDF->currentOffSet += nChars;
    pTail += nChars;
    pPDF->pdfObj++;

    /* Catalog Object. This is the Last Object */
    jpeg2pdf_setxref(pPDF, INDEX_USE_PPDF, pPDF->currentOffSet, 'n');
    nChars = sprintf(pTail, "%d 0 obj\n<</Type/Catalog /Pages 1 0 R /Metadata %d 0 R>>\nendobj\n", pPDF->pdfObj, metadataObj);

    pPDF->currentOffSet += nChars;
    pTail += nChars;
		
    /* Pages Object. It's always the Object 1 */
    jpeg2pdf_setxref(pPDF, 1, pPDF->currentOffSet, 'n');

    strKids[0] = 0;
    pNode = pPDF->pFirstNode;

    while (pNode != NULL) {
      char curStr[9];
      sprintf(curStr, "%d 0 R ", pNode->PageObj);
      strcat(strKids, curStr);
      pNode = pNode->pNext;
    }

    if(strlen(strKids) > 1 && strKids[strlen(strKids) - 1] == ' ') strKids[strlen(strKids) - 1] = 0;
		
    nChars = sprintf(pTail, "1 0 obj\n<</Type/Pages /Kids [%s] /Count %d>>\nendobj\n", strKids, pPDF->nodeCount);

    pPDF->currentOffSet += nChars;
    pTail += nChars;

    /* The xref & the rest of the tail */
    xrefOffSet = pPDF->currentOffSet;
    nChars = sprintf(pTail, "xref\n0 %d\n", pPDF->pdfObj+1);
    pPDF->currentOffSet += nChars;
    pTail += nChars;

    for (i = 0; i <= pPDF->pdfObj; i++) {
      nChars = sprintf(pTail, "%s", pPDF->pdfXREF[i]);
      pPDF->currentOffSet += nChars;
      pTail += nChars;
    }

    /* write trailer */
    nChars = sprintf(pTail, "trailer\n<<\n/Root %d 0 R\n/Info %d 0 R\n/Size %d\n>>\n", pPDF->pdfObj, infoObj, pPDF->pdfObj+1);

    pPDF->currentOffSet += nChars;
    pTail += nChars;
    nChars = sprintf(pTail, "startxref\n%d\n%%%%EOF\n", xrefOffSet);
    pPDF->currentOffSet += nChars;
    pTail += nChars;
  }
  headerSize = strlen(pPDF->pdfHeader);
  tailerSize = strlen(pPDF->pdfTailer);

  if (headerSize && tailerSize &&
	(pPDF->currentOffSet > headerSize + tailerSize)) {
    pdfSize = pPDF->currentOffSet;
  }
  return pdfSize;
} /* </jpeg2pdf_metadata> */

/*
* jpeg2pdf_finalize
*/
int
jpeg2pdf_finalize(jpeg2pdf_ptr_t pPDF, guint8 *outPDF, guint32 *outPDFSize)
{
  int result = Error;

  if (pPDF) {
    jpeg2pdf_node_ptr_t pNode, pFreeCurrent;

    if (outPDF && (*outPDFSize >= pPDF->currentOffSet)) {
      guint32 nBytes, nBytesOut = 0;
      guint8 *pOut = outPDF;

      nBytes = strlen((char *)pPDF->pdfHeader);
      memcpy(pOut, pPDF->pdfHeader, nBytes);
      nBytesOut += nBytes;
      pOut += nBytes;
      pNode = (jpeg2pdf_node_ptr_t)pPDF->pFirstNode;

      while(pNode != NULL) {
	nBytes = strlen(pNode->preFormat);
	memcpy(pOut, pNode->preFormat, nBytes);
	nBytesOut += nBytes; pOut += nBytes;

	nBytes = pNode->jpegSize;
	memcpy(pOut, pNode->pJpeg, nBytes);
	nBytesOut += nBytes; pOut += nBytes;

	nBytes = strlen((char *)pNode->pstFormat);
	memcpy(pOut, pNode->pstFormat, nBytes);
	nBytesOut += nBytes; pOut += nBytes;

	pNode = pNode->pNext;
      }

      nBytes = strlen((char *)pPDF->pdfTailer);
      memcpy(pOut, pPDF->pdfTailer, nBytes);
      nBytesOut += nBytes; pOut += nBytes;

      *outPDFSize = nBytesOut;
      result = Success;
    }
    pNode = pPDF->pFirstNode;

    while(pNode != NULL) {
      if(pNode->pJpeg) free(pNode->pJpeg);
      pFreeCurrent = pNode;
      pNode = pNode->pNext;
      free(pFreeCurrent);
    }

    if (pPDF != NULL) {
      free(pPDF);
      pPDF = NULL;
    }
  }
  return result;
} /* </jpeg2pdf_finalize> */

/*
* get_jpeg_size - Gets the JPEG size from the array of data passed
*                 to the function, file reference:
*
*                 http://www.obrador.com/essentialjpeg/headerinfo.htm
*/
bool
get_jpeg_size(unsigned char *data, unsigned int data_size,
              unsigned short *width, unsigned short *height,
              unsigned char *colors, double *dpiX, double *dpiY)
{
  int pos = 0;   // Keeps track of the position within the file

  /* Check for valid JPEG image */
  if (data[pos] == 0xFF && data[pos+1] == 0xD8 && data[pos+2] == 0xFF ) {
    pos += 4;
    /* Check for valid JPEG header (null terminated JFIF)
    if (data[pos+2] == 'J' && data[pos+3] == 'F' && data[pos+4] == 'I'
                           && data[pos+5] == 'F' && data[pos+6] == 0x00) {
    */
      /* Retrieve dpi:
      *  It is also possible to retrieve "rational" dpi
      *  from EXIF data -- in that case it'll be really double.
      */
      /* Should we prefer EXIF data when present? */
      guint8 units = data[pos+9];

      if (units == 1) {      // pixels per inch
        *dpiX = data[pos+10] * 256+data[pos+11]; // Xdensity
        *dpiY = data[pos+12] * 256+data[pos+13]; // Ydensity
      }
      else if (units == 2) { // pixels per cm
        *dpiX = (data[pos+10] * 256+data[pos+11]) * 2.54; // Xdensity => dpiX
        *dpiY = (data[pos+12] * 256+data[pos+13]) * 2.54; // Ydensity => dpiY
      }
      else { // units==0, fallback to 300dpi? Here EXIF data would be useful.
        *dpiX = *dpiY = 300;
      }
      /*
      * Retrieve the block length of the first block since
      * the first block will not contain the size of file.
      */
      unsigned short block_length = data[pos] * 256 + data[pos+1];
      while (pos < (int)data_size) {
        pos += block_length; // Increase the file index to get to the next block

        // Check to protect against segmentation faults
        if(pos >= (int)data_size) return false;

        // Check that we are truly at the start of another block
        if(data[pos] != 0xFF) return false;

        if (data[pos+1] >= 0xC0 && data[pos+1] <= 0xC2) {
          /*
          * 0xFFC0 is the "Start of frame" marker which contains the file size
          * The structure of the 0xFFC0 block is quite simple
          * [0xFFC0][ushort length][uchar precision][ushort x][ushort y]
          */
          *height = data[pos+5]*256 + data[pos+6];
          *width = data[pos+7]*256 + data[pos+8];
          *colors = data[pos+9];
          return true;
        }
        else {
          pos += 2;  // Skip the block marker
          block_length = data[pos] * 256 + data[pos+1]; // Go to the next block
        }
      }
      return false; // If this point is reached then no size was found
     //} else { return false; } // Not a valid JFIF string
  } else { return false; } // Not a valid SOI header
} /* </get_jpeg_size> */
