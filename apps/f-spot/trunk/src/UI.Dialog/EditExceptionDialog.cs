/*
 * FSpot.UI.Dialog.EditExceptionDialog.cs
 *
 * Author(s)
 *	Larry Ewing <lewing@novell.com>
 *
 * This is free software. See COPYING for details.
 */

using System;
using Mono.Unix;
using Gtk;

namespace FSpot.UI.Dialog
{
	public class EditException : Exception 
	{
		IBrowsableItem item;

		public IBrowsableItem Item {
			get { return item; }
		}

		public EditException (IBrowsableItem item, Exception e) : base (
                        String.Format (Catalog.GetString ("Received exception \"{0}\". Unable to save photo {1}"),
				       e.Message, item.Name), e)
		{
			this.item = item;
		}
	}

	public class EditExceptionDialog : HigMessageDialog
	{
		private const int MaxErrors = 10;

		public EditExceptionDialog (Gtk.Window parent, Exception [] errors) : base (parent, DialogFlags.DestroyWithParent,
											    Gtk.MessageType.Error, ButtonsType.Ok,
											    Catalog.GetString ("Error editing photo"),
											    GenerateMessage (errors))
		{
			foreach (Exception e in errors)
				Console.WriteLine (e);
		}

		public EditExceptionDialog (Gtk.Window parent, Exception e, IBrowsableItem item) : this (parent, new EditException (item, e))
		{
		}
	       
		public EditExceptionDialog (Gtk.Window parent, Exception e) : this (parent, new Exception [] { e })
		{
		}

		private static string GenerateMessage (Exception [] errors)
		{
			string desc = String.Empty;
			for (int i = 0; i < errors.Length && i < MaxErrors; i++) {
				Exception e = errors [i];
				desc += e.Message + Environment.NewLine;
			}
			return desc;
		}
	}
}
