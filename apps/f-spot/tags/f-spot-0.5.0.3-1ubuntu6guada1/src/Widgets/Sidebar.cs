/*
 * Widgets.Sidebar.cs
 *
 * Author(s)
 * 	Mike Gemuende <mike@gemuende.de>
 *	Stephane Delcroix <stephane@delcroix.org>
 *	Ruben Vermeersch <ruben@savanne.be>
 *
 * This is free software. See COPYING for details.
 */

using FSpot.Extensions;
using FSpot.Utils;
using Gtk;
using Mono.Addins;
using Mono.Unix;
using System;
using System.Collections.Generic;

namespace FSpot.Widgets {
	[ExtensionNode ("SidebarPage")]
	public class SidebarPageNode : ExtensionNode {
		[NodeAttribute (Required=true)]
		protected string sidebar_page_type;

		public SidebarPage GetSidebarPage () {
			return (SidebarPage) Addin.CreateInstance (sidebar_page_type);
		}
	}

	// Decides which sidebar page should be shown for each context. Implemented
	// using the Strategy pattern, to make it swappable easily, in case the 
	// default MRUSidebarContextSwitchStrategy is not sufficiently usable.
	public abstract class SidebarContextSwitchStrategy {
		private readonly Sidebar Sidebar;

		public SidebarContextSwitchStrategy (Sidebar sidebar) {
			Sidebar = sidebar;
		}

		public abstract string PageForContext (ViewContext context);

		public abstract void SwitchedToPage (ViewContext context, string name);
	}

	// Implements a Most Recently Used switching strategy. The last page you used
	// for a given context is used.
	public class MRUSidebarContextSwitchStrategy : SidebarContextSwitchStrategy {
		public const string PREF_PREFIX = Preferences.APP_FSPOT + "ui/sidebar";

		public MRUSidebarContextSwitchStrategy (Sidebar sidebar) : base (sidebar) {
		}

		private string PrefKeyForContext (ViewContext context) {
			return String.Format ("{0}/{1}", PREF_PREFIX, context);
		}

		public override string PageForContext (ViewContext context) {
			string name = Preferences.Get<string> (PrefKeyForContext (context));
			if (name == null) 
				name = DefaultForContext (context);
			return name;
		}

		public override void SwitchedToPage (ViewContext context, string name) {
			Preferences.Set (PrefKeyForContext (context), name);
		}

		private string DefaultForContext (ViewContext context) {
			if (context == ViewContext.Edit)
				return Catalog.GetString ("Edit");
			// Don't care otherwise, Tags sounds reasonable
			return Catalog.GetString ("Tags");
		}
	}

	public class SidebarPage {
		// The widget shown on the sidebar page.
		private readonly Widget widget;
		public Widget SidebarWidget {
			get { return widget; }
		}

		// Whether this page can be selected
		private bool can_select;
		public bool CanSelect {
			protected set { 
				can_select = value;
				if (CanSelectChanged != null)
					CanSelectChanged (this, null);
			}
			get { return can_select; }
		}

		public event EventHandler CanSelectChanged;

		// The label of the sidebar page.
		private readonly string label;
		public string Label {
			get { return label; }
		}

		// The icon name, used for the selector
		private readonly string icon_name;
		public string IconName {
			get { return icon_name; }
		}

		// The sidebar onto which this page is attached
		private Sidebar sidebar;
		public Sidebar Sidebar {
			get { return sidebar; }
			set { 
				sidebar = value; 
				AddedToSidebar ();
			}
		}

		// Can be overriden to get notified as soon as we're added to a sidebar.
		protected virtual void AddedToSidebar () { }

		// Whether this page is currently visible
		public bool IsActive {
			get { return Sidebar.IsActive (this); }
		}

		public SidebarPage (Widget widget, string label, string icon_name) {
			this.widget = widget;
			this.label = label;
			this.icon_name = icon_name;
		}
	}

	public class Sidebar : VBox  {
		
		private HBox button_box;
		private Notebook notebook;
		private MenuButton choose_button;
		private EventBox eventBox;
		private Menu choose_menu;
		private List<string> menu_list;
		private List<string> image_list;

		private List<SidebarPage> pages;

		public event EventHandler CloseRequested;

        	// Selection change events, sidebar pages can subscribed to this.
		public event IBrowsableCollectionChangedHandler SelectionChanged;
		public event IBrowsableCollectionItemsChangedHandler SelectionItemsChanged;

		// The photos selected.
		private IBrowsableCollection selection;
		public IBrowsableCollection Selection {
			get { return selection; }
			private set { selection = value; }
		}

		public event EventHandler ContextChanged;

		private ViewContext view_context = ViewContext.Unknown;
		public ViewContext Context {
			get { return view_context; }
			set {
				view_context = value;
				if (ContextChanged != null)
					ContextChanged (this, null);
			}
		}

		private readonly SidebarContextSwitchStrategy ContextSwitchStrategy;

		public Sidebar () : base ()
		{
			ContextSwitchStrategy = new MRUSidebarContextSwitchStrategy (this);
			ContextChanged += HandleContextChanged;

			button_box = new HBox ();
			PackStart (button_box, false, false, 0);
			
			notebook = new Notebook ();
			notebook.ShowTabs = false;
			notebook.ShowBorder = false;
			PackStart (notebook, true, true, 0);
			
			Button button = new Button ();
			button.Image = new Image ("gtk-close", IconSize.Button);
			button.Relief = ReliefStyle.None;
			button.Pressed += HandleCloseButtonPressed;
			button_box.PackEnd (button, false, true, 0);
			
			choose_button = new MenuButton ();
			choose_button.Relief = ReliefStyle.None;
			
			eventBox = new EventBox ();
			eventBox.Add (choose_button);
			
			button_box.PackStart (eventBox, true, true, 0);
			
			choose_menu = new Menu ();
			choose_button.Menu = choose_menu;

			menu_list = new List<string> ();
			image_list = new List<string> ();
			pages = new List<SidebarPage> ();
		}

		private void HandleContextChanged (object sender, EventArgs args)
		{
			// Make sure the ViewModeCondition is set correctly.
			if (Context == ViewContext.Single)
				ViewModeCondition.Mode = FSpot.Extensions.ViewMode.Single;
			else if (Context == ViewContext.Library || Context == ViewContext.Edit)
				ViewModeCondition.Mode = FSpot.Extensions.ViewMode.Library;
			else
				ViewModeCondition.Mode = FSpot.Extensions.ViewMode.Unknown;

			string name = ContextSwitchStrategy.PageForContext (Context);
			SwitchTo (name);
		}

		private void HandleCanSelectChanged (object sender, EventArgs args)
		{
			//Log.DebugFormat ("Can select changed for {0} to {1}", sender, (sender as SidebarPage).CanSelect);
		}

		public void AppendPage (Widget widget, string label, string icon_name)
		{
			AppendPage (new SidebarPage (widget, label, icon_name));
		}
		
        public void AppendPage (SidebarPage page)
		{	
			page.Sidebar = this;
			page.CanSelectChanged += HandleCanSelectChanged;
			pages.Add (page);

			string label = page.Label;
			string icon_name = page.IconName;

			notebook.AppendPage (page.SidebarWidget, new Label (label));
			page.SidebarWidget.Show ();
			
			MenuItem item; 
			if (icon_name == null)
				item = new MenuItem (label);
			else {
				item = new ImageMenuItem (label);
				(item as ImageMenuItem).Image = new Image ();
				((item as ImageMenuItem).Image as Image).IconName = icon_name;
			}

			item.Activated += HandleItemClicked;
			choose_menu.Append (item);
			item.Show ();
			
			if (notebook.Children.Length == 1) {
				choose_button.Label = label;
				choose_button.Image.IconName = icon_name;
			}
			menu_list.Add (label);
			image_list.Add (icon_name);
		}

		public void HandleMainWindowViewModeChanged (object o, EventArgs args)
		{
			MainWindow.ModeType mode = MainWindow.Toplevel.ViewMode;
			if (mode == MainWindow.ModeType.IconView) 
				Context = ViewContext.Library;
			else if (mode == MainWindow.ModeType.PhotoView)
				Context = ViewContext.Edit;
		}
		
		public void HandleItemClicked (object o, EventArgs args)
		{
			string name = ((o as MenuItem).Child as Label).Text;
			SwitchTo (name);
			ContextSwitchStrategy.SwitchedToPage (Context, name);
		}
		
		public void HandleCloseButtonPressed (object sender, EventArgs args)
		{
			if (CloseRequested != null)
				CloseRequested (this, args);
		}
		
		public void SwitchTo (int n)
		{
			if (n >= notebook.Children.Length) {
				n = 0;
			}

			notebook.CurrentPage = n;
			choose_button.Label = menu_list [n];
			choose_button.Image.IconName = image_list [n];
		}

		public int CurrentPage
		{
			get { return notebook.CurrentPage; }
		}

		public void SwitchTo (string name)
		{
			if (menu_list.Contains (name))
				SwitchTo (menu_list.IndexOf (name));
		}
		
		public bool IsActive (SidebarPage page)
		{
			return (notebook.GetNthPage (notebook.CurrentPage) == page.SidebarWidget);
		}

		public void HandleSelectionChanged (IBrowsableCollection collection) {
			Selection = collection;
        		// Proxy selection change to the subscribed sidebar pages.
			if (SelectionChanged != null) 
				SelectionChanged (collection);
		}

		// Proxy selection item changes to the subscribed sidebar pages.
		public void HandleSelectionItemsChanged (IBrowsableCollection collection, BrowsableEventArgs args) {
			if (SelectionItemsChanged != null)
				SelectionItemsChanged (collection, args);
		}
	} 
}
