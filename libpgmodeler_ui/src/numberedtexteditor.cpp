/*
# PostgreSQL Database Modeler (pgModeler)
#
# Copyright 2006-2016 - Raphael Araújo e Silva <raphael@pgmodeler.com.br>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation version 3.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# The complete text of GPLv3 is at LICENSE file on source code root directory.
# Also, you can get the complete GNU General Public License at <http://www.gnu.org/licenses/>
*/

#include "numberedtexteditor.h"
#include "generalconfigwidget.h"
#include <QTextBlock>
#include <QScrollBar>
#include <QDebug>

bool NumberedTextEditor::line_nums_visible=true;
bool NumberedTextEditor::highlight_lines=true;
QColor NumberedTextEditor::line_hl_color=Qt::yellow;
QFont NumberedTextEditor::default_font=QFont(QString("DejaVu Sans Mono"), 10);
int NumberedTextEditor::tab_width=0;

NumberedTextEditor::NumberedTextEditor(QWidget * parent) : QPlainTextEdit(parent)
{
	line_number_wgt=new LineNumbersWidget(this);
	setWordWrapMode(QTextOption::NoWrap);
	setContextMenuPolicy(Qt::CustomContextMenu);

	connect(this, SIGNAL(cursorPositionChanged()), this, SLOT(highlightCurrentLine()));
	connect(this, SIGNAL(updateRequest(QRect,int)), this, SLOT(updateLineNumbers(void)));
	connect(this, SIGNAL(blockCountChanged(int)), this, SLOT(updateLineNumbersSize()));
	connect(this, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showContextMenu()));
}

void NumberedTextEditor::setDefaultFont(const QFont &font)
{
	default_font=font;
}

void NumberedTextEditor::setLineNumbersVisible(bool value)
{
	line_nums_visible=value;
}

void NumberedTextEditor::setHighlightLines(bool value)
{
	highlight_lines=value;
}

void NumberedTextEditor::setLineHighlightColor(const QColor &color)
{
	line_hl_color=color;
}

void NumberedTextEditor::setTabWidth(int value)
{
	if(value < 0)
		tab_width=0;
	else
		tab_width=value;
}

int NumberedTextEditor::getTabWidth(void)
{
	if(tab_width == 0)
		return(80);
	else
	{
		QFontMetrics fm(default_font);
		return(tab_width * fm.width(' '));
	}
}

void NumberedTextEditor::showContextMenu(void)
{
	QMenu *ctx_menu;
	QAction *act=nullptr;

	ctx_menu=createStandardContextMenu();

	if(!isReadOnly())
	{
		ctx_menu->addSeparator();

		act=ctx_menu->addAction(trUtf8("Upper case"), this, SLOT(changeSelectionToUpper()), QKeySequence(QString("Ctrl+U")));
		act->setEnabled(textCursor().hasSelection());

		act=ctx_menu->addAction(trUtf8("Lower case"), this, SLOT(changeSelectionToLower()), QKeySequence(QString("Ctrl+Shift+U")));
		act->setEnabled(textCursor().hasSelection());

		ctx_menu->addSeparator();

		act=ctx_menu->addAction(trUtf8("Ident right"), this, SLOT(identSelectionRight()), QKeySequence(QString("Tab")));
		act->setEnabled(textCursor().hasSelection());

		act=ctx_menu->addAction(trUtf8("Ident left"), this, SLOT(identSelectionLeft()), QKeySequence(QString("Shift+Tab")));
		act->setEnabled(textCursor().hasSelection());
	}

	ctx_menu->exec(QCursor::pos());
	delete(ctx_menu);
}

void NumberedTextEditor::changeSelectionToLower(void)
{
	changeSelectionCase(true);
}

void NumberedTextEditor::changeSelectionToUpper(void)
{
	changeSelectionCase(false);
}

void NumberedTextEditor::changeSelectionCase(bool lower)
{
	QTextCursor cursor=textCursor();

	if(cursor.hasSelection())
	{
		int start=cursor.selectionStart(),
				end=cursor.selectionEnd();

		if(!lower)
			cursor.insertText(cursor.selectedText().toUpper());
		else
			cursor.insertText(cursor.selectedText().toLower());

		cursor.setPosition(start);
		cursor.setPosition(end, QTextCursor::KeepAnchor);
		setTextCursor(cursor);
	}
}

void NumberedTextEditor::identSelectionRight(void)
{
	identSelection(true);
}

void NumberedTextEditor::identSelectionLeft(void)
{
	identSelection(false);
}

void NumberedTextEditor::identSelection(bool ident_right)
{
	QTextCursor cursor=textCursor();

	if(cursor.hasSelection())
	{
		QStringList lines;
		int start=-1,	end=-1,
				factor=(ident_right ? 1 : -1),	count=0;

		/* Forcing the selection of the very beggining of the first line and
		as well the end of the last line to avoid moving chars and break words wrongly */
		start=toPlainText().lastIndexOf(QChar('\n'), cursor.selectionStart());
		end=toPlainText().indexOf(QChar('\n'), cursor.selectionEnd());

		cursor.setPosition(start, QTextCursor::MoveAnchor);
		cursor.setPosition(end, QTextCursor::KeepAnchor);
		lines=cursor.selectedText().split(QChar(QChar::ParagraphSeparator));

		for(int i=0; i < lines.size(); i++)
		{
			if(!lines[i].isEmpty())
			{
				if(ident_right)
				{
					lines[i].prepend(QChar('\t'));
					count++;
				}
				else if(lines[i].at(0)==QChar('\t'))
				{
					lines[i].remove(0,1);
					count++;
				}
			}
		}

		if(count > 0)
		{
			cursor.insertText(lines.join(QChar('\n')));

			//Preserving the selection in the text to permit user perform several identations
			cursor.setPosition(start);
			cursor.setPosition(end + (count * factor), QTextCursor::KeepAnchor);
			setTextCursor(cursor);
		}
	}
}

void NumberedTextEditor::setFocus(void)
{
	QPlainTextEdit::setFocus();
	this->highlightCurrentLine();
}

void NumberedTextEditor::updateLineNumbers(void)
{
	line_number_wgt->setVisible(line_nums_visible);
	if(!line_nums_visible) return;

	setFont(default_font);
	line_number_wgt->setFont(default_font);

	QTextBlock block = firstVisibleBlock();
	int block_number = block.blockNumber(),
			//Calculates the first block postion (in widget coordinates)
			top = static_cast<int>(blockBoundingGeometry(block).translated(contentOffset()).top()),
			bottom = top +  static_cast<int>(blockBoundingRect(block).height()),
			dy = top;
	unsigned first_line=0, line_count=0;

	// Calculates the visible lines by iterating over the visible/valid text blocks.
	while(block.isValid())
	{
		if(block.isVisible())
		{
			line_count++;
			if(first_line==0)
				first_line=static_cast<unsigned>(block_number + 1);
		}

		block = block.next();
		top = bottom;
		bottom = top + static_cast<int>(blockBoundingRect(block).height());
		++block_number;

		/* Check if the line count converted to widget coordinates is higher than the widget height.
	   This is done to avoid draw line numbers that are beyond the widget's height */
		if((static_cast<int>(line_count) * fontMetrics().height()) > this->height())
			break;
	}

	line_number_wgt->drawLineNumbers(first_line, line_count, dy);

	if(this->tabStopWidth()!=NumberedTextEditor::getTabWidth())
		this->setTabStopWidth(NumberedTextEditor::getTabWidth());
}

void NumberedTextEditor::updateLineNumbersSize(void)
{
	if(line_nums_visible)
	{
		QRect rect=contentsRect();
		setViewportMargins(getLineNumbersWidth(), 0, 0, 0);
		line_number_wgt->setGeometry(QRect(rect.left(), rect.top(), getLineNumbersWidth(), rect.height()));
	}
	else
		setViewportMargins(0, 0, 0, 0);
}

int NumberedTextEditor::getLineNumbersWidth(void)
{
	int digits=1, max=qMax(1, blockCount());

	while(max >= 10)
	{
		max /= 10;
		++digits;
	}

	return(15 + fontMetrics().width(QChar('9')) * digits);
}

void NumberedTextEditor::resizeEvent(QResizeEvent *event)
{
	QPlainTextEdit::resizeEvent(event);
	updateLineNumbersSize();
}

void NumberedTextEditor::keyPressEvent(QKeyEvent *event)
{
	if(!isReadOnly() && textCursor().hasSelection())
	{
		if(event->key()==Qt::Key_U && event->modifiers()!=Qt::NoModifier)
		{
			if(event->modifiers()==Qt::ControlModifier)
				changeSelectionToUpper();
			else if(event->modifiers()==(Qt::ControlModifier | Qt::ShiftModifier))
				changeSelectionToLower();
		}
		else if(event->key()==Qt::Key_Tab || event->key()==Qt::Key_Backtab)
		{
			if(event->key()==Qt::Key_Tab)
				identSelectionRight();
			else if(event->key()==Qt::Key_Backtab)
				identSelectionLeft();
		}
		else
			QPlainTextEdit::keyPressEvent(event);
	}
	else
		QPlainTextEdit::keyPressEvent(event);
}

void NumberedTextEditor::highlightCurrentLine(void)
{
	QList<QTextEdit::ExtraSelection> extraSelections;

	if(highlight_lines && !isReadOnly())
	{
		QTextEdit::ExtraSelection selection;
		selection.format.setBackground(line_hl_color);
		selection.format.setProperty(QTextFormat::FullWidthSelection, true);
		selection.cursor = textCursor();
		selection.cursor.clearSelection();
		extraSelections.append(selection);
	}

	setExtraSelections(extraSelections);
}
