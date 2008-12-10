<?xml version="1.0"?>
<!DOCTYPE xsl:stylesheet [
<!ENTITY css SYSTEM "mutt.css">
]>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
  <xsl:param name="section.autolabel" select="1"></xsl:param>
  <xsl:template name="user.head.content">
    <xsl:element name="style">
      <xsl:attribute name="type">text/css</xsl:attribute>
      &css;
    </xsl:element>
  </xsl:template>
  <xsl:param name="admon.style"/>
</xsl:stylesheet>
